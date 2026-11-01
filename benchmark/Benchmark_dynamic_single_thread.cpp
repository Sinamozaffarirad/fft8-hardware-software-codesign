#include <iostream>
#include <complex>
#include <cmath>
#include <cassert>
#include <cstdlib> 
#include <chrono>
#include <iomanip>
#include <queue>
#include <random>
#include <array>
#include <vector>
#include <mutex>
using namespace std;

const long long int MAXFIFO = 180000000;
#define MAXIO 8

typedef complex<double> cplx;

// FIFO Structure 
struct fifo_t {
    vector<cplx> data;
    long long int wptr;
    long long int rptr;

    fifo_t(long long int size = MAXFIFO) : data(size), wptr(0), rptr(0) {}
};

inline void init_fifo(fifo_t *F) {
    F->wptr = F->rptr = 0;
    fill(F->data.begin(), F->data.end(), cplx(0.0, 0.0));
}

inline long long int fifo_size(fifo_t *F) {
    if (F->wptr >= F->rptr)
        return F->wptr - F->rptr;
    else
        return MAXFIFO - (F->rptr - F->wptr);
}

inline void put_fifo(fifo_t *F, cplx d) {
    long long int next = (F->wptr + 1) % MAXFIFO;
    if (next == F->rptr) {
        cerr << "Error: FIFO is full, cannot add element." << endl;
        return;
    }
    F->data[F->wptr] = d;
    F->wptr = next;
    assert(fifo_size(F) <= MAXFIFO);
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

// Reorder actor
auto reorder_fire = [](actorio_t *g) -> bool {
    bool fired = false;
    while (fifo_size(g->in[0]) >= 8) {
        cplx inbuf[8];
        int rev[8] = {0,4,2,6,1,5,3,7};
        for (int i=0; i<8; i++) inbuf[i] = get_fifo(g->in[0]);
        for (int i=0; i<8; i++) put_fifo(g->out[0], inbuf[rev[i]]);
        fired = true;
    }
    return fired;
};

// FFT2 actor
auto fft2_fire = [](actorio_t *g) -> bool {
    bool fired = false;
    while (fifo_size(g->in[0]) >= 2) {
        cplx a = get_fifo(g->in[0]);
        cplx b = get_fifo(g->in[0]);
        put_fifo(g->out[0], a+b);
        put_fifo(g->out[0], a-b);
        fired = true;
    }
    return fired;
};

// FFT4 actor
auto fft4_fire = [](actorio_t *g) -> bool {
    bool fired = false;
    while (fifo_size(g->in[0]) >= 4) {
        cplx inb[4];
        for (int i=0; i<4; i++) inb[i] = get_fifo(g->in[0]);
        cplx w4_1(0.0, -1.0);
        cplx X0 = inb[0] + inb[2];
        cplx X2 = inb[0] - inb[2];
        cplx X1 = inb[1] + inb[3]*w4_1;
        cplx X3 = inb[1] - inb[3]*w4_1;

        put_fifo(g->out[0], X0);
        put_fifo(g->out[0], X1);
        put_fifo(g->out[0], X2);
        put_fifo(g->out[0], X3);
        fired = true;
    }
    return fired;
};

// FFT8 + Magnitude actor
auto fft8mag_fire = [](actorio_t *g) -> bool { 
    bool fired = false;
    while (fifo_size(g->in[0]) >= 8) {
        // Compute FFT8
        cplx inb[8];
        for (int i = 0; i < 8; i++) inb[i] = get_fifo(g->in[0]);

        cplx outb[8];
        for (int k = 0; k < 4; k++) {
            double angle = -2.0*PI*k/8.0;
            cplx w(cos(angle), sin(angle));
            cplx u = inb[k];
            cplx v = inb[k+4]*w;
            outb[k]   = u+v;
            outb[k+4] = u-v;
        }

        fired = true;
    }
    return fired;
};

int main(int argc, char* argv[]) {

    // Start Total Runtime Timer 
    auto total_start = chrono::high_resolution_clock::now(); 

    // Number of FFT8 inputs in queue
    const int NUM_FFT8 = 3000000;
    queue<array<cplx, 8>> input_queue;

    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis_real(-10.0, 10.0);
    uniform_real_distribution<> dis_imag(-10.0, 10.0);

    for (int set = 0; set < NUM_FFT8; set++) {
        array<cplx, 8> input_set;
        for (int i = 0; i < 8; i++) {
            double re = dis_real(gen);
            double im = dis_imag(gen);
            input_set[i] = cplx(re, im);
        }
        input_queue.push(input_set);
    }
    // Default size MAXFIFO
    fifo_t inputF;       
    fifo_t reorderF;
    fifo_t fft2F;
    fifo_t fft4F;

    init_fifo(&inputF);
    init_fifo(&reorderF);
    init_fifo(&fft2F);
    init_fifo(&fft4F);

    while (!input_queue.empty()) {
        array<cplx, 8> current_set = input_queue.front();
        input_queue.pop();
        for (int i = 0; i < 8; i++) {
            put_fifo(&inputF, current_set[i]);
        }
    }

    actorio_t reorder_actor = {{&inputF},{&reorderF}};
    actorio_t fft2_actor    = {{&reorderF},{&fft2F}};
    actorio_t fft4_actor    = {{&fft2F},{&fft4F}};
    actorio_t fft8mag_actor = {{&fft4F},{}};

    // Record start time before processing
    auto benchmark_start = chrono::high_resolution_clock::now();

    for (int set = 0; set < NUM_FFT8; set++) { 
        bool fired = false;
        fired |= reorder_fire(&reorder_actor);
        fired |= fft2_fire(&fft2_actor);
        fired |= fft4_fire(&fft4_actor);
        fired |= fft8mag_fire(&fft8mag_actor);

        if (!fired) {
            break;
        }
    }

    // Record end time after processing
    auto benchmark_end = chrono::high_resolution_clock::now();
    chrono::duration<double, std::micro> total_duration_us = benchmark_end - benchmark_start;

    cout << fixed << setprecision(2);
    cout << "Benchmarking Results for " << NUM_FFT8 << " FFT8 Operations:" << endl;
    cout << "Total Processing Time: " << total_duration_us.count() / 1000.0 << " ms" << endl;
    cout << "Average FFT8 Processing Time: " << (total_duration_us.count() / NUM_FFT8) << " us per FFT8" << endl;

    // Total Runtime Timer End 
    auto total_end = chrono::high_resolution_clock::now(); 
    chrono::duration<double, std::micro> total_runtime_us = total_end - total_start; 

    cout << "Total Runtime of Program: " << (total_runtime_us.count() / 1000.0) << " ms" << endl; 

    return 0;
}
