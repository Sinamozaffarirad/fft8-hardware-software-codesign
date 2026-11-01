#include <iostream>
#include <complex>
#include <cmath>
#include <cstdlib>  
using namespace std;

int main(int argc, char* argv[]) {
    typedef complex<double> cplx;
    const double PI = 3.14159265358979323846;

    cplx input[8];
    if (argc == 17) {
        for (int i = 0; i < 8; i++) {
            double re = atof(argv[2*i + 1]); //converts string to double
            double im = atof(argv[2*i + 2]);
            input[i] = cplx(re, im);
        }
    } else {
        // Default hardcoded values if no arguments are given
        input[0] = cplx(1.0, 0.0);
        input[1] = cplx(1.0, 0.0);
        input[2] = cplx(1.0, 0.0);
        input[3] = cplx(1.0, 0.0);
        input[4] = cplx(0.0, 0.0);
        input[5] = cplx(0.0, 0.0);
        input[6] = cplx(0.0, 0.0);
        input[7] = cplx(0.0, 0.0);
    }

    // Reorder stage (bit reversal for N=8)
    int rev[8] = {0,4,2,6,1,5,3,7};
    cplx reorder_out[8];
    for (int i=0; i<8; i++){
        reorder_out[i] = input[rev[i]];
    }

    // First stage of FFT8 (FFT2 stage)
    cplx fft2_out[8];
    for (int i=0; i<4; i++) { //W_2^k = cos(2*pi*k/2) - j*sin(2*pi*k/2)
        cplx a = reorder_out[2*i];
        cplx b = reorder_out[2*i+1];
        fft2_out[2*i]   = a+b;
        fft2_out[2*i+1] = a-b;
    }

    // Second stage of FFT8 (FFT4 stage) 
    cplx fft4_out[8];
    {
        cplx w4_1(0.0, -1.0); //W_4^k = cos(2*pi*k/4) - j*sin(2*pi*k/4)

        // Block 0..3
        {
            cplx inb[4] = {fft2_out[0], fft2_out[1], fft2_out[2], fft2_out[3]};
            cplx X0 = inb[0]+inb[2];
            cplx X2 = inb[0]-inb[2];
            cplx X1 = inb[1] + inb[3]*w4_1;
            cplx X3 = inb[1] - inb[3]*w4_1;
            fft4_out[0] = X0; fft4_out[1] = X1; fft4_out[2] = X2; fft4_out[3] = X3;
        }

        // Block 4..7
        {
            cplx inb[4] = {fft2_out[4], fft2_out[5], fft2_out[6], fft2_out[7]};
            cplx X0 = inb[0]+inb[2];
            cplx X2 = inb[0]-inb[2];
            cplx X1 = inb[1] + inb[3]*w4_1;
            cplx X3 = inb[1] - inb[3]*w4_1;
            fft4_out[4] = X0; fft4_out[5] = X1; fft4_out[6] = X2; fft4_out[7] = X3;
        }
    }

    // Third stage of FFT8 (FFT8 stage)
    cplx fft8_out[8];
    for (int k=0; k<4; k++) { //W_8^k = cos(2*pi*k/8) - j*sin(2*pi*k/8)
        double angle = -2.0*PI*k/8.0;
        cplx w(cos(angle), sin(angle));
        cplx u = fft4_out[k];
        cplx v = fft4_out[k+4]*w;
        fft8_out[k]   = u+v;
        fft8_out[k+4] = u-v;
    }

    cout << "Final FFT8 Complex Results and Magnitudes:" << endl;
    for (int i=0; i<8; i++){
        double re = fft8_out[i].real();
        double im = fft8_out[i].imag();
        double mag = sqrt(re*re + im*im);
        cout << "X[" << i << "] = " << re << " + j" << im
             << " , Magnitude = " << mag << endl;
    }

    return 0;
}
