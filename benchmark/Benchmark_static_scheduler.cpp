#include <iostream>
#include <complex>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <queue>
#include <random>
#include <array>
#include <vector>
using namespace std;

int main(int argc, char* argv[]) {
    // Start the total runtime timer
    auto total_start = chrono::high_resolution_clock::now();

    typedef complex<double> cplx;
    const double PI = 3.14159265358979323846;

    const int QUEUE_SIZE = 3000000;

    int num_input_sets = 1;
    bool use_random_inputs = false;
    if (argc > 1) {
        if ((argc - 1) % 16 != 0) {
            cerr << "Error: Number of arguments must be 16 per input set (8 complex numbers)." << endl;
            return 1;
        }
        num_input_sets = (argc - 1) / 16;
    } else {
        use_random_inputs = true;
        num_input_sets = QUEUE_SIZE;
    }

    int input_counter = 1;

    queue<array<cplx, 8>> input_queue;

    if (use_random_inputs) {
        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<> dis_real(-10.0, 10.0);
        uniform_real_distribution<> dis_imag(-10.0, 10.0);

        for (int set = 0; set < QUEUE_SIZE; set++) {
            array<cplx, 8> input_set;
            for (int i = 0; i < 8; i++) {
                double re = dis_real(gen);
                double im = dis_imag(gen);
                input_set[i] = cplx(re, im);
            }
            input_queue.push(input_set);
        }
    }

    // Start the global benchmark timer
    auto benchmark_start = chrono::high_resolution_clock::now();

    for (int set = 0; set < num_input_sets; set++) {
        array<cplx, 8> input_set;

        if (!use_random_inputs) {
            for (int i = 0; i < 8; i++) {
                double re = atof(argv[set * 16 + 1 + 2 * i]);
                double im = atof(argv[set * 16 + 2 + 2 * i]);
                input_set[i] = cplx(re, im);
            }
        } else {
            if (input_queue.empty()) {
                cerr << "Error: Input queue is empty unexpectedly." << endl;
                return 1;
            }
            input_set = input_queue.front();
            input_queue.pop();
        }

        // Reorder Stage
        int rev[8] = {0, 4, 2, 6, 1, 5, 3, 7};
        array<cplx, 8> reorder_out;
        for (int i = 0; i < 8; i++) {
            reorder_out[i] = input_set[rev[i]];
        }

        // FFT2 Stage
        array<cplx, 8> fft2_out;
        for (int i = 0; i < 4; i++) {
            cplx a = reorder_out[2 * i];
            cplx b = reorder_out[2 * i + 1];
            fft2_out[2 * i] = a + b;
            fft2_out[2 * i + 1] = a - b;
        }

        // FFT4 Stage
        array<cplx, 8> fft4_out;
        {
            cplx w4_1(0.0, -1.0);

            {
                array<cplx, 4> inb = {fft2_out[0], fft2_out[1], fft2_out[2], fft2_out[3]};
                cplx X0 = inb[0] + inb[2];
                cplx X2 = inb[0] - inb[2];
                cplx X1 = inb[1] + inb[3] * w4_1;
                cplx X3 = inb[1] - inb[3] * w4_1;
                fft4_out[0] = X0;
                fft4_out[1] = X1;
                fft4_out[2] = X2;
                fft4_out[3] = X3;
            }

            {
                array<cplx, 4> inb = {fft2_out[4], fft2_out[5], fft2_out[6], fft2_out[7]};
                cplx X0 = inb[0] + inb[2];
                cplx X2 = inb[0] - inb[2];
                cplx X1 = inb[1] + inb[3] * w4_1;
                cplx X3 = inb[1] - inb[3] * w4_1;
                fft4_out[4] = X0;
                fft4_out[5] = X1;
                fft4_out[6] = X2;
                fft4_out[7] = X3;
            }
        }

        // FFT8 Stage
        array<cplx, 8> fft8_out;
        for (int k = 0; k < 4; k++) {
            double angle = -2.0 * PI * k / 8.0;
            cplx w(cos(angle), sin(angle));
            cplx u = fft4_out[k];
            cplx v = fft4_out[k + 4] * w;
            fft8_out[k] = u + v;
            fft8_out[k + 4] = u - v;
        }

        input_counter++;
    }

    auto benchmark_end = chrono::high_resolution_clock::now();
    chrono::duration<double, micro> total_duration_us = benchmark_end - benchmark_start;

    if (num_input_sets > 0) {
        double total_time_us = total_duration_us.count();
        double average_time_us = total_time_us / num_input_sets;

        cout << fixed << setprecision(2);
        cout << "Benchmarking Results for " << num_input_sets << " FFT8 Operations:" << endl;
        cout << "Total Processing Time: " << total_time_us / 1000.0 << " ms" << endl;
        cout << "Average FFT8 Processing Time: " << average_time_us << " us per FFT8" << endl;
    } else {
        cout << "No FFT8 operations were processed." << endl;
    }

    // End the total runtime timer
    auto total_end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> total_runtime_ms = total_end - total_start;

    cout << fixed << setprecision(2);
    cout << "Total Runtime of Program: " << total_runtime_ms.count() << " ms" << endl;

    return 0;
}
