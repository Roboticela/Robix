# Benchmarks

These files are **local synthetic kernels** written for this repository. Each workload is a short loop with a Robix source, a C source, and a C++ source. They measure `robixc -O3` against Clang and Clang++ on that loop.

They are **not** upstream benchmark suites.

| Folder | What a reader might assume | What it is |
| :--- | :--- | :--- |
| `DaCapoSuite/` | The DaCapo Benchmark Suite | Local kernels whose file names recall DaCapo workloads |
| `DebianBenchmarksGame/` | The Computer Language Benchmarks Game | Local kernels whose file names recall that game |
| `NasParallelBenchmarks/` | The NAS Parallel Benchmarks | Local kernels whose file names recall NPB kernels |
| `SpecCpu2026/` | SPEC CPU | Local kernels whose file names recall SPEC workloads |
| `NativeVsC/` | A microbenchmark set | Local microbenchmarks. This folder name is accurate |

No folder vendors SPEC, DaCapo, NPB, or the Benchmarks Game. Do not quote a timing from this tree as a result of those suites. `upstream` is false until a real upstream archive is added and `RunBenchmarks.ps1` is pointed at it.

## How a number may be quoted

A timing is quotable only next to all of the following, which `RunBenchmarks.ps1` prints with every run:

- `robixc --version`
- `clang --version` and `clang++ --version` (first line)
- Flags: `robixc -O3`, `clang -O3 -march=native`, `clang++ -O3 -march=native`
- The checksum line printed by each of the three binaries
- The interleaved average: two warmups, then Robix, C, and C++ timed in rotation, highest sample dropped when there are at least four samples

A full sweep heats a laptop CPU. The script's own average is the interleaved figure for that process. It is not a separate cool confirmation. Before citing a win or a loss, run that one workload again on an idle machine and keep the checksum line beside the time.

If the three checksum lines differ, the timing is not a comparison. The script marks that row `checksum mismatch`.

## Runner

```text
Benchmarks\RunBenchmarks.ps1 -Suite NativeVsC -Runs 5
```

Suite names `DaCapo`, `Debian`, `NPB`, and `SPEC` select folders. The banner states that those folders are synthetic.
