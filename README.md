# FFT8 Hardware-Software Co-Design

Student course project exploring scheduling choices for an 8-point radix-2 Fast Fourier Transform (FFT). The project expresses the transform as a small streaming pipeline and implements three execution approaches around the same computational stages.

## Project focus

The implementation models the FFT as a sequence of actors connected by FIFO channels:

1. bit-reversal reordering;
2. radix-2 butterfly stage;
3. radix-4 butterfly stage; and
4. final radix-8 combination with magnitude calculation.

The repository compares three scheduling strategies:

| Implementation | Approach |
| --- | --- |
| `static_scheduler` | Direct, statically ordered execution of the FFT stages. |
| `dynamic_single_thread` | FIFO-driven actor scheduling in one thread. |
| `dynamic_multi_thread` | FIFO-driven actors coordinated across threads with round-robin scheduling. |

The `benchmark/` directory contains the original benchmarking implementations and a process-level benchmark driver for comparing the executable variants. Benchmark configuration and resource requirements are intentionally left as supplied by the student project; no benchmark result is claimed in this repository unless it is reproduced in the target environment.

## Repository layout

```text
src/
  static_scheduler.cpp          Static FFT8 schedule
  dynamic_single_thread.cpp     Dynamic single-thread scheduler
  dynamic_multi_thread.cpp      Dynamic multi-thread scheduler
benchmark/
  Benchmark_*.cpp               Per-strategy benchmark implementations
  fft_benchmark.cpp             Executable-level benchmark driver
CMakeLists.txt                  CMake build definition
```

## Build

Requirements: a C++17 compiler and CMake 3.16 or newer.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Run any of the three FFT implementations without arguments to use the included sample input:

```powershell
.\build\Release\static_scheduler.exe
.\build\Release\dynamic_single_thread.exe
.\build\Release\dynamic_multi_thread.exe
```

Each program also accepts sixteen numeric command-line values: real and imaginary components for eight complex input samples, in order.

## Notes on provenance and scope

This is a student submission for a Hardware-Software Co-Design course. The source files in `src/` and `benchmark/` are the complete final source set selected from the original project folders, preserved without changes to their algorithmic implementation. Repository metadata, build configuration, and this documentation were added solely to make the work reproducible and suitable for public review.

The repository intentionally excludes generated executables, IDE settings, build directories, duplicate intermediate versions, and archive files. Those artifacts do not improve code review or reproducibility because the project can be rebuilt from the included source.
