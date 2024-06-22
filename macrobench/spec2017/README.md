## Benchmark Execution Guide

SPECCPU installation with iso.
* sudo mount -t iso9660 -o ro,exec,loop [iso path(ex:cpu2017.iso)] [mount directory (ex:/mnt)]
* cd [mount directory (ex:/mnt)] && ./install.sh
* enter ~/SPECCPU_2017 and y

1. To begin, ensure all required dependencies are installed. 

* Install SPECPU2017 in the home directory.(~/SPECCPU_2017)
* Modify the `monitor_wrapper` in the `spec_config.cfg` to personal directory.
* Modify the `output_csv` route in the `bench_spec.sh`(if not set, then saved in SPEC install directory).
* Modify the `LABEL` to proper label(ex: glibc, mbpf, ...) in `bench_spec.sh`.

2. Executes the benchmark by executing the following command in your terminal:

```bash
(Linux Native) ./bench_spec.sh
(mBPF) sudo LD_PRELOAD="libkernel.so" ./bench_spec.sh
(...)
```

* For specific benchmarks monitor_wrapper evoke errors! (`503.bwaves_r, 603.bwaves_s`). For these benchmarks please comment out `monitor_wrapper` in `spec_config.cfg`

This command executes the benchmark and directs the results to the `benchmark_results.csv` file.
