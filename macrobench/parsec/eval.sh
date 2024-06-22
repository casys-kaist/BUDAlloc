#!/bin/bash

function kill_command {
    # If $benchmark_command doesn't exist correctly(SIGSEGV, SIGABRT), some process run's infinitly
    # Therefore, we should kill these processes
    ps -ef | grep parsec | grep $benchmark_name | grep -v bench_parsec | awk {'print $2'} | xargs -r kill -9
}

# Command to run the benchmark is passed as arguments to this script
benchmark_command="$@"

# File to store the output of /usr/bin/time
# benchmark_name=$(echo "$(basename "$benchmark_command")" | cut -d ' ' -f 1)
current_pid=$$
time_output_file=$DIR_RESULT/$benchmark_name.time.out.$current_pid

HOME="/home/${USER_NAME}"

echo "eval.sh: $BENCH_TARGET"

case $BENCH_TARGET in
    "BUDAlloc")
        export LD_PRELOAD=libkernel.so
        ;;
    "ffmalloc")
        export LD_PRELOAD="$HOME/ffmalloc/libffmallocnpmt.so"
        ;;
    "markus")
        export LD_PRELOAD="$HOME/markus-allocator/lib/libgc.so $HOME/markus-allocator/lib/libgccpp.so"
        ;;
    "dangzero")
        export LD_PRELOAD="/trusted/wrap.so"
        ;;
    * )
        ;;
esac

# Run the benchmark with /usr/bin/time -v and capture the output
{ /usr/bin/time -v $benchmark_command 2>&1; kill_command; } | tee $time_output_file