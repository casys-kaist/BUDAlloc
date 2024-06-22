#!/bin/bash

# Get the name of the benchmark from the command line arguments
benchmark_name=$(basename "$(dirname "$(dirname "$(pwd)")")")

# File to store the output of /usr/bin/time
current_pid=$$
time_output_file=$DIR_RESULT/$benchmark_name.time.out.$current_pid

# Command to run the benchmark is passed as arguments to this script
benchmark_command="$@"
if [ $SUDO_USER ]; then
    HOME="/home/$SUDO_USER"
else
    HOME="/home/$(whoami)"
fi

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
/usr/bin/time -v $benchmark_command 2>> $time_output_file
