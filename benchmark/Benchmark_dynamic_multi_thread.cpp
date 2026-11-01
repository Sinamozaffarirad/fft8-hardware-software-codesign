#include <iostream>
#include <complex>
#include <cmath>
#include <cassert>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <random>
#include <iomanip>
#include <array>
using namespace std;

// Hyperparameters
#define TOTAL_RUNS 1000      // Total number of benchmarking runs
#define QUEUE_SIZE 3000      // Number of FFT8 input sets per run

#define MAXFIFO 25000
#define MAXIO 8
typedef complex<double> cplx;

// FIFO Structure 
struct fifo_t {
    cplx data[MAXFIFO];
    unsigned wptr;
    unsigned rptr;
};

inline void init_fifo(fifo_t *F) {
    F->wptr = F->rptr = 0;
}

inline int fifo_size(fifo_t *F) {
    if (F->wptr >= F->rptr)
        return F->wptr - F->rptr;
    else
        return MAXFIFO - (F->rptr - F->wptr);
}

inline void put_fifo(fifo_t *F, cplx d) {
    unsigned next = (F->wptr + 1) % MAXFIFO;
    assert(next != F->rptr && "FIFO Error: FIFO is full, cannot add element."); 
    F->data[F->wptr] = d;
    F->wptr = next;
}

inline cplx get_fifo(fifo_t *F) {
    assert(fifo_size(F) > 0 && "Attempting to get from empty FIFO");
    cplx r = F->data[F->rptr];
    F->rptr = (F->rptr + 1) % MAXFIFO;
    return r;
}

// Actor I/O Structure
struct actorio_t {
    fifo_t *in[MAXIO];
    fifo_t *out[MAXIO];
};

// Global Benchmarking Variable
static const double PI = 3.14159265358979323846;

static std::mutex sched_mtx;
static std::condition_variable sched_cv;
static std::atomic<bool> stop_all{false};
static vector<thread> threads;
static int current_thread = 0;
static int thread_count = 0;

// Counter for processed FFT8 operations
static atomic<int> fft8_processed{0};

// Round-robin yield
inline void stp_yield(int myid) {
    {
        unique_lock<mutex> lk(sched_mtx);
        // Move to next thread
        int next_thread = (myid + 1) % thread_count;
        current_thread = next_thread;
        sched_cv.notify_all();

        while (!stop_all && current_thread != myid) {
            sched_cv.wait(lk);
        }
    }
}

// Reorder actor
void reorder_actor_fun(actorio_t *g, int myid) {
    while (!stop_all) {
        {
            unique_lock<mutex> lk(sched_mtx);
            while (!stop_all && current_thread != myid) {
                sched_cv.wait(lk);
            }
        }
        while (fifo_size(g->in[0]) >= 8) {
            cplx inbuf[8];
            int rev[8] = {0,4,2,6,1,5,3,7};
            for (int i=0; i<8; i++) inbuf[i] = get_fifo(g->in[0]);
            for (int i=0; i<8; i++) put_fifo(g->out[0], inbuf[rev[i]]);
        }
        stp_yield(myid);
    }
}

// FFT2 actor
void fft2_actor_fun(actorio_t *g, int myid) {
    while (!stop_all) {
        {
            unique_lock<mutex> lk(sched_mtx);
            while (!stop_all && current_thread != myid) {
                sched_cv.wait(lk);
            }
        }
        while (fifo_size(g->in[0]) >= 2) { //W_2^k = cos(2*pi*k/2) - j*sin(2*pi*k/2) = 1 or -1
            cplx a = get_fifo(g->in[0]);
            cplx b = get_fifo(g->in[0]);
            put_fifo(g->out[0], a+b); //W_2^k = 1
            put_fifo(g->out[0], a-b); //W_2^k = -1
        }
        stp_yield(myid);
    }
}

// FFT4 actor
void fft4_actor_fun(actorio_t *g, int myid) {
    while (!stop_all) {
        {
            unique_lock<mutex> lk(sched_mtx);
            while (!stop_all && current_thread != myid) {
                sched_cv.wait(lk);
            }
        }
        while (fifo_size(g->in[0]) >= 4) {
            cplx inb[4];
            for (int i=0; i<4; i++) inb[i] = get_fifo(g->in[0]);
            cplx w4_1(0.0, -1.0); //W_4^k = cos(2*pi*k/4) - j*sin(2*pi*k/4)
            cplx X0 = inb[0] + inb[2]; //Even inputs: W_4^0 = 1, W_4^2 = -1
            cplx X2 = inb[0] - inb[2];
            cplx X1 = inb[1] + inb[3]*w4_1; //Odd inputs: W_4^1 = j, W_4^3 = -j
            cplx X3 = inb[1] - inb[3]*w4_1;

            put_fifo(g->out[0], X0);
            put_fifo(g->out[0], X1);
            put_fifo(g->out[0], X2);
            put_fifo(g->out[0], X3);
        }
        stp_yield(myid);
    }
}

// FFT8+Magnitude actor
void fft8mag_actor_fun(actorio_t *g, int myid) {
    while (!stop_all) {
        {
            unique_lock<mutex> lk(sched_mtx);
            while (!stop_all && current_thread != myid) {
                sched_cv.wait(lk);
            }
        }
        while (fifo_size(g->in[0]) >= 8) {
            // Compute FFT8
            cplx inb[8];
            for (int i=0; i<8; i++) inb[i] = get_fifo(g->in[0]);

            cplx outb[8];
            for (int k=0; k<4; k++) {
                double angle = -2.0*PI*k/8.0;
                cplx w(cos(angle), sin(angle)); //W_8^k = cos(2*pi*k/8) - j*sin(2*pi*k/8)
                cplx u = inb[k]; // FFT4 result (even)
                cplx v = inb[k+4]*w; // FFT4 result (odd) * twiddle factor
                outb[k]   = u+v;
                outb[k+4] = u-v;
            }

            int processed = ++fft8_processed;

            if (processed >= QUEUE_SIZE) { 
                stop_all.store(true);
                {
                    lock_guard<mutex> lk(sched_mtx);
                    sched_cv.notify_all();
                }
                break;
            }
        }
        stp_yield(myid);
    }
}

int stp_create(void (*f)(actorio_t*,int), actorio_t* arg) {
    int myid = thread_count++;
    threads.emplace_back(f, arg, myid);
    return myid;
}

void stp_init() {
    threads.clear();
    thread_count = 0;
    stop_all.store(false);
    fft8_processed.store(0);
}

void stp_start() {
    {
        lock_guard<mutex> lk(sched_mtx);
        current_thread = 0;
        sched_cv.notify_all();
    }
    for (auto &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
}

int main(int argc, char* argv[]) {

    // Start the total runtime timer
    auto program_start = chrono::high_resolution_clock::now();

    std::mt19937_64 rng;
    rng.seed(std::random_device()());
    std::uniform_real_distribution<double> dist(-10.0, 10.0); 


    vector<pair<double, double>> benchmark_results;
    benchmark_results.reserve(TOTAL_RUNS);

    double total_time_sum_ms = 0.0;
    double average_fft8_time_sum_us = 0.0;

    for(int run = 1; run <= TOTAL_RUNS; run++) {

        // FIFOs
        fifo_t inputF, reorderF, fft2F, fft4F;
        init_fifo(&inputF);
        init_fifo(&reorderF);
        init_fifo(&fft2F);
        init_fifo(&fft4F);

        vector<cplx> base_input_data;
   
        base_input_data.reserve(8 * QUEUE_SIZE); 
        for (int set = 0; set < QUEUE_SIZE; set++) {
            for (int i = 0; i < 8; i++) {
                double re = dist(rng);
                double im = dist(rng);
                base_input_data.emplace_back(re, im);
            }
        }

        for (const auto& c : base_input_data) {
            put_fifo(&inputF, c);
        }

        actorio_t reorder_actor = {{&inputF},{&reorderF}};
        actorio_t fft2_actor    = {{&reorderF},{&fft2F}};
        actorio_t fft4_actor    = {{&fft2F},{&fft4F}};
        actorio_t fft8mag_actor = {{&fft4F},{}};

        stp_init();
        // Create threads
        stp_create(reorder_actor_fun, &reorder_actor);
        stp_create(fft2_actor_fun,    &fft2_actor);
        stp_create(fft4_actor_fun,    &fft4_actor);
        stp_create(fft8mag_actor_fun, &fft8mag_actor);

        // Record start time before starting processing
        auto benchmark_start = chrono::high_resolution_clock::now();

        // Start processing
        stp_start();

        // Record end time after processing
        auto benchmark_end = chrono::high_resolution_clock::now();
        chrono::duration<double, std::micro> total_duration_us = benchmark_end - benchmark_start;

        double average_fft8_time_us = total_duration_us.count() / QUEUE_SIZE;

        benchmark_results.emplace_back(total_duration_us.count() / 1000.0, average_fft8_time_us);

        total_time_sum_ms += (total_duration_us.count() / 1000.0);
        average_fft8_time_sum_us += average_fft8_time_us;

        if(run % 100 == 0){
            cout << "Completed " << run << " runs." << endl;
        }
    }

    // Calculate overall averages
    double overall_average_total_time_ms = total_time_sum_ms / TOTAL_RUNS;
    double overall_average_fft8_time_us = average_fft8_time_sum_us / TOTAL_RUNS;

    // Calculate total FFT8 operations
    long long total_fft8_operations = static_cast<long long>(QUEUE_SIZE) * TOTAL_RUNS;

    cout << fixed << setprecision(2);
    cout << "\nBenchmarking Results for " << TOTAL_RUNS << " Runs of " << QUEUE_SIZE << " FFT8 Operations Each:" << endl;
    cout << "Run\tTotal Processing Time (ms)\tAverage FFT8 Processing Time (us)" << endl;

    for(int run = 0; run < TOTAL_RUNS; run++) {
        cout << (run + 1) << "\t" 
             << benchmark_results[run].first << "\t\t\t" 
             << benchmark_results[run].second << endl;
    }

    cout << "\nOverall Total Processing Time: " << total_time_sum_ms << " ms" << endl;

    cout << "Total FFT8 Operations: " << total_fft8_operations << endl;

    cout << "Overall Averages Across All Runs:" << endl;
    cout << "Average Total Processing Time: " << overall_average_total_time_ms << " ms" << endl;
    cout << "Average FFT8 Processing Time: " << overall_average_fft8_time_us << " us per FFT8" << endl;

    // End the total runtime timer
    auto program_end = chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> program_duration_ms = program_end - program_start;

    cout << "Total Runtime of Program: " << program_duration_ms.count() << " ms" << endl;

    return 0;
}
