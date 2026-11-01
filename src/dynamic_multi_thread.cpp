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
using namespace std;

#define MAXFIFO 1024
#define MAXIO 8
typedef complex<double> cplx;

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
    assert(next != F->rptr); 
    F->data[F->wptr] = d;
    F->wptr = next;
}

inline cplx get_fifo(fifo_t *F) {
    assert(fifo_size(F) > 0 && "Attempting to get from empty FIFO");
    cplx r = F->data[F->rptr];
    F->rptr = (F->rptr + 1) % MAXFIFO;
    return r;
}

struct actorio_t {
    fifo_t *in[MAXIO];
    fifo_t *out[MAXIO];
};

static const double PI = 3.14159265358979323846;
static std::mutex sched_mtx;
static std::condition_variable sched_cv;
static std::atomic<bool> stop_all{false};
static vector<thread> threads;
static int current_thread = 0;
static int thread_count = 0;

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
        bool fired = false;
        while (fifo_size(g->in[0]) >= 8) {
            cplx inbuf[8];
            int rev[8] = {0,4,2,6,1,5,3,7};
            for (int i=0; i<8; i++) inbuf[i] = get_fifo(g->in[0]);
            for (int i=0; i<8; i++) put_fifo(g->out[0], inbuf[rev[i]]);
            fired = true;
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
            cplx w4_1(0.0, -1.0);  //W_4^k = cos(2*pi*k/4) - j*sin(2*pi*k/4)
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
        if (fifo_size(g->in[0]) >= 8) {
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

            cout << "Final FFT8 Results and Magnitudes:" << endl;
            for (int i=0; i<8; i++) {
                double re = outb[i].real();
                double im = outb[i].imag();
                double mag = sqrt(re*re + im*im);
                cout << "X[" << i << "] = " << re << " + j" << im
                     << " , Magnitude = " << mag << endl;
            }

            // Stop all threads after processing 8 samples
            stop_all.store(true);
            {
                lock_guard<mutex> lk(sched_mtx);
                sched_cv.notify_all();
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
}

void stp_start() {
    {
        lock_guard<mutex> lk(sched_mtx);
        current_thread = 0;
        sched_cv.notify_all();
    }
    for (auto &th : threads) {
        th.join();
    }
}

int main(int argc, char* argv[]) {

    cplx input_data[8];
    if (argc == 17) {
        for (int i=0; i<8; i++) {
            double re = atof(argv[2*i + 1]);
            double im = atof(argv[2*i + 2]);
            input_data[i] = cplx(re, im);
        }
    } else {
        input_data[0] = cplx(1.0,0.0);
        input_data[1] = cplx(1.0,0.0);
        input_data[2] = cplx(1.0,0.0);
        input_data[3] = cplx(1.0,0.0);
        input_data[4] = cplx(0.0,0.0);
        input_data[5] = cplx(0.0,0.0);
        input_data[6] = cplx(0.0,0.0);
        input_data[7] = cplx(0.0,0.0);
    }

    // FIFOs
    fifo_t inputF, reorderF, fft2F, fft4F;
    init_fifo(&inputF);
    init_fifo(&reorderF);
    init_fifo(&fft2F);
    init_fifo(&fft4F);

    // Put data
    for (int i=0; i<8; i++) {
        put_fifo(&inputF, input_data[i]);
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

    // Round-robin
    stp_start();

    return 0;
}
