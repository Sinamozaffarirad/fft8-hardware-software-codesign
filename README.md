# FFT8 Scheduling for Hardware-Software Co-Design

An 8-point radix-2 Fast Fourier Transform (FFT) implemented in C++ for a Hardware-Software Co-Design course project. The work compares a direct static schedule with FIFO-based dynamic scheduling in single-threaded and multi-threaded forms.

![FFT8 dataflow architecture](assets/fft8-pipeline.svg)

## Architecture

The transform is decomposed into four dataflow stages:

1. Bit-reversal reordering of the input sequence
2. FFT2 butterfly stage
3. FFT4 butterfly stage
4. FFT8 combination and magnitude calculation

Complex samples are represented with `std::complex<double>`. In the dynamic implementations, FIFO channels connect the stages and control when an actor may execute.

## Implementations

| Program | Scheduling model |
| --- | --- |
| `static_scheduler` | Fixed execution order with the FFT stages evaluated directly. |
| `dynamic_single_thread` | FIFO-driven actor execution in one thread. |
| `dynamic_multi_thread` | One actor per thread, coordinated with a condition variable and round-robin scheduling. |

The `benchmark/` directory contains the supplied benchmark variants for the three scheduling models, together with an executable-level benchmark driver.

## Verification

The supplied executables were run with the default input:

```text
[1 + j0, 1 + j0, 1 + j0, 1 + j0, 0 + j0, 0 + j0, 0 + j0, 0 + j0]
```

All three implementations completed with exit code `0` and produced the same output.

```text
Bin    Complex output                    Magnitude
X[0]   4.000000 + j0.000000              4.000000
X[1]   1.000000 - j2.414214              2.613126
X[2]   0.000000 + j0.000000              0.000000
X[3]   1.000000 - j0.414214              1.082392
X[4]   0.000000 + j0.000000              0.000000
X[5]   1.000000 + j0.414214              1.082392
X[6]   0.000000 + j0.000000              0.000000
X[7]   1.000000 + j2.414214              2.613126
```

The table is a functional correctness check; it is not a performance result.

## Build and run

Requirements: CMake 3.16+ and a C++17 compiler.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

```powershell
.\build\Release\static_scheduler.exe
.\build\Release\dynamic_single_thread.exe
.\build\Release\dynamic_multi_thread.exe
```

Each executable accepts 16 numeric arguments: the real and imaginary components of eight complex input samples.

## Repository layout

```text
src/        FFT8 implementations
benchmark/  Benchmark implementations and driver
assets/     Architecture diagram
```
