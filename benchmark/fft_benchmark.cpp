#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <cstdlib>  
#include <ctime>    
#include <iomanip> 
#include <sstream>   
#include <thread>
#include <future>

#define PATH_SEPARATOR '\\'

using namespace std;
using namespace std::chrono;

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    cout << fixed << setprecision(2);

    // Paths to executables in the current directory
    // Adjust the paths as necessary 
    string static_scheduler_exe;
    string dynamic_single_thread_exe;
    string dynamic_multi_thread_exe;

    static_scheduler_exe      = ".\\static_scheduler.exe";
    dynamic_single_thread_exe = ".\\dynamic_single_thread.exe";
    dynamic_multi_thread_exe  = ".\\dynamic_multi_thread.exe";


    // Number of random tests in the queue
    const int NUM_TESTS = 100;

    vector<vector<double>> queue_reals(NUM_TESTS, vector<double>(8));
    vector<vector<double>> queue_imags(NUM_TESTS, vector<double>(8));

    for (int t = 0; t < NUM_TESTS; t++) {
        for (int i = 0; i < 8; i++) {
            queue_reals[t][i] = static_cast<double>(rand()) / RAND_MAX * 2.0 - 1.0; // Range: [-1, 1]
            queue_imags[t][i] = static_cast<double>(rand()) / RAND_MAX * 2.0 - 1.0; // Range: [-1, 1]
        }
    }

    cout << "Created a queue of " << NUM_TESTS << " random FFT8 inputs.\n\n";

    struct BenchmarkResult {
        string name;
        double total_time_us;
        double average_time_us;
    };
    vector<BenchmarkResult> results;

    struct Implementation {
        string name;
        string exe_path;
    };

    vector<Implementation> implementations = {
        {"Static Scheduler Inline", static_scheduler_exe},
        {"Dynamic Scheduler Single Thread", dynamic_single_thread_exe},
        {"Dynamic Scheduler Multi Thread", dynamic_multi_thread_exe}
    };

    for (const auto& impl : implementations) {
        cout << "Running entire queue of " << NUM_TESTS
             << " inputs on " << impl.name << "...\n";

        double total_time = 0.0;

        for (int t = 0; t < NUM_TESTS; t++) {
            ostringstream cmd_stream;
            cmd_stream << "\"" << impl.exe_path << "\"";
            for (int i = 0; i < 8; i++) {
                cmd_stream << " " << queue_reals[t][i] << " " << queue_imags[t][i];
            }
            string cmd = cmd_stream.str();

            auto start = steady_clock::now();
            auto future = std::async(std::launch::async, [&cmd]() {
                return system(cmd.c_str());
            });

            auto status = future.wait_for(std::chrono::seconds(10));
            auto end = steady_clock::now();

            if (status != std::future_status::ready || future.get() != 0) {
            microseconds elapsed = duration_cast<microseconds>(end - start);
                cerr << "Command: " << cmd << "\n";
                // Skip timing for this failed input
                continue;
            }

            duration<double, micro> elapsed = end - start;
            total_time += elapsed.count();
        }

        double average_time = (NUM_TESTS > 0) ? (total_time / NUM_TESTS) : 0.0;

        results.push_back(BenchmarkResult{impl.name, total_time, average_time});

        cout << "Completed " << impl.name << ".\n\n";
    }

    cout << "FFT8 Benchmarking Results for a queue of " << NUM_TESTS << " inputs:\n";
    cout << "-----------------------------------------------------------------\n";
    for (const auto& res : results) {
        cout << res.name << " : total " << res.total_time_us
             << " us,  average " << res.average_time_us << " us per input\n";
    }
    cout << "-----------------------------------------------------------------\n";

    return 0;
}
