## Benchmark Execution Guide

1. To begin, ensure all required dependencies are installed. 

* Install the parsec by `install.sh`

2. Executes the benchmark by executing the following command in your terminal:

```bash
./bench_parsec.sh
```

* It asks you number of threads and size of benchmark.

* To change the preload library, you have to change the "$PRELOAD" variable in the `bench_parsec.sh`.
* Default setting is the `sudo LD_PRELOAD="libkernel.so"`.

After running you will get result.log and benchmark_results.csv