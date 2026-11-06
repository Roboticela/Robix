# Benchmarks

`RunBenchmarks.ps1` times local kernels in `Micro/`, `Numeric/`, and `Systems/`. Each kernel is a Robix file, a C file, and a C++ file with the same trip count. The three binaries must print the same checksum or the row is not a comparison.

The measured score is `Micro/README.md`. That file lists wins, ties, losses, and rows the cool sample did not confirm.

```text
powershell -File Benchmarks\RunBenchmarks.ps1 -Suite All -Runs 5
powershell -File Benchmarks\RunBenchmarks.ps1 -Suite Micro
```

Flags the script prints and uses: `robixc -O3`, `clang -O3 -march=native`, `clang++ -O3 -march=native`.

Two warmups, five rounds with the starting language rotated, highest sample dropped. A time within 5 percent of the fastest five-run average is a tie. A win or a loss is published only when a later cool sample agrees on direction. A five-run tie is not overturned by one cool sample. A loss that both passes show stays a loss.

`-Suite All` is Micro, Numeric, and Systems. Any other suite name is refused, and the refusal does not echo that name, unless that directory's `manifest.txt` contains `upstream: true`. Every manifest in this tree says `upstream: false`.
