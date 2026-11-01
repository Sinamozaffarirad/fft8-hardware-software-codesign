#include <iostream>
#include <complex>
#include <cmath>
#include <cassert>
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

struct actorio_t {
    fifo_t *in[MAXIO];
    fifo_t *out[MAXIO];
};

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
    while (fifo_size(g->in[0]) >= 2) { //W_2^k = cos(2*pi*k/2) - j*sin(2*pi*k/2) = 1 or -1
        cplx a = get_fifo(g->in[0]);
        cplx b = get_fifo(g->in[0]);
        put_fifo(g->out[0], a+b); //W_2^k = 1
        put_fifo(g->out[0], a-b); //W_2^k = -1
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
        cplx w4_1(0.0, -1.0); //W_4^k = cos(2*pi*k/4) - j*sin(2*pi*k/4)
        cplx X0 = inb[0] + inb[2]; //Even inputs: W_4^0 = 1, W_4^2 = -1
        cplx X2 = inb[0] - inb[2];
        cplx X1 = inb[1] + inb[3]*w4_1;  //Odd inputs: W_4^1 = j, W_4^3 = -j
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
        fired = true;
    }
    return fired;
};

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

    // Create FIFOs
    fifo_t inputF, reorderF, fft2F, fft4F;
    init_fifo(&inputF);
    init_fifo(&reorderF);
    init_fifo(&fft2F);
    init_fifo(&fft4F);

    // Push input_data into inputF
    for (int i=0; i<8; i++) {
        put_fifo(&inputF, input_data[i]);
    }

    // Setup actor I/O
    actorio_t reorder_actor = {{&inputF},{&reorderF}};
    actorio_t fft2_actor    = {{&reorderF},{&fft2F}};
    actorio_t fft4_actor    = {{&fft2F},{&fft4F}};
    actorio_t fft8mag_actor = {{&fft4F},{}};

    // Dynamic single-thread scheduling
    while (true) {
        bool fired = false;
        // Arbitrary order
        fired |= fft4_fire(&fft4_actor);
        fired |= fft8mag_fire(&fft8mag_actor);
        fired |= fft2_fire(&fft2_actor);
        fired |= reorder_fire(&reorder_actor);

        if (!fired) {
            break;
        }
    }

    return 0;
}
