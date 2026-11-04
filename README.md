# FFT8: Static vs Dynamic (Dataflow) Scheduling in C++

A project for the Hardware/Software Co-Design course at the University of Kashan.
Sina Mozaffari Rad.

I wrote an 8-point radix-2 FFT in C++ as a dataflow graph of four actors, and then ran the same graph with three
different schedulers. I wanted to see in practice the difference between a fixed static schedule and a dynamic,
FIFO-driven one, which is a central topic in co-design: the same dataflow model can be scheduled in software or
mapped to hardware stages.

![FFT8 dataflow architecture](assets/fft8-pipeline.svg)

## The dataflow graph

The FFT is split into four stages (actors):

1. bit-reversal reordering of the input,
2. FFT2 butterflies,
3. FFT4 butterflies,
4. FFT8 combination and the magnitude of each output.

Samples are `std::complex<double>`. In the dynamic versions the actors are connected by FIFO channels, and an
actor can only fire when there is enough data in its input FIFO.

## The three schedulers

| Program | How it is scheduled |
| --- | --- |
| `static_scheduler` | A fixed order, decided before running; the stages are just called one after the other. |
| `dynamic_single_thread` | One thread checks the FIFOs and fires whichever actor is ready. |
| `dynamic_multi_thread` | One thread per actor, synchronised with a condition variable, with round-robin turns. |

For an 8-point FFT on a PC I expect the static schedule to be the fastest, because the threads and FIFOs cost
more than the arithmetic itself. The point of the dynamic versions is that they follow the dataflow model directly,
so they show how the same graph would behave as independent hardware stages connected by buffers.

The `benchmark/` folder has a benchmark version of each scheduler and a driver that runs them.

## Checking the output

I ran all three programs with this input:

```text
[1 + j0, 1 + j0, 1 + j0, 1 + j0, 0 + j0, 0 + j0, 0 + j0, 0 + j0]
```

All three finished with exit code `0` and gave the same result, which is the correct DFT of this input:

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

This only checks correctness. I have not put timing numbers here.

## What I would add

- Timing results from the benchmark driver, for many runs, in a table.
- A larger FFT (64 or 1024 points), where the dynamic multi-threaded version could actually benefit from
  pipelining several frames.
- A version of one stage in HLS or VHDL, to compare a hardware actor with the software one.

## Build and run

You need CMake 3.16+ and a C++17 compiler.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

```powershell
.\build\Release\static_scheduler.exe
.\build\Release\dynamic_single_thread.exe
.\build\Release\dynamic_multi_thread.exe
```

Each program also accepts 16 numbers as arguments: the real and imaginary parts of the eight input samples.

## Files

```text
src/        The three FFT8 schedulers
benchmark/  Benchmark versions and the driver
assets/     Dataflow diagram
```
