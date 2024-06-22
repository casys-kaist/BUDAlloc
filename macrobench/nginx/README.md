## Benchmark Execution Guide

1. To begin, ensure all required dependencies are installed. 

* Install the nginx by `install.sh`

2. Executes the nginx by executing the following command in your terminal:
(You can change number of worker_processes by changing WORKER=[num] in run_nginx.sh)

```bash
./run_nginx.sh
```
* To change the preload library, you have to change the "$PRELOAD" variable in the `run_nginx.sh`.
* Default setting is the `sudo LD_PRELOAD="libkernel.so"`.

3. Executes the benchmark by executing the following command in your terminal:

```bash
./bench_nginx.sh
```
This output results to results.csv.
