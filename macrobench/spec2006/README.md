## Benchmark Execution Guide

1. To begin, ensure all required dependencies are installed. 

* Install SPECPU2006 in the home directory.
* Modify the `dir_result` in the `spec_config.cfg` to personal directory(please let this directory be the directory where eval.sh locates in).

2. Executes the benchmark by executing the following command in your terminal:

```bash
(Linux Native) ./bench_spec.sh
(mBPF) sudo LD_PRELOAD="libkernel.so" ./bench_spec.sh
(...)
```

## Understanding the Results
You can find the all benchmark results in the benchmark_results.csv file.