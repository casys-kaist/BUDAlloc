#!/bin/bash

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--LIBCS=value] [--THREADS=value]"
            exit 0
            ;;
        --LIBCS=*)
            LIBCS="${1#*=}"
            shift
            ;;
        --THREADS=*)
            THREADS="${1#*=}"
            shift
            ;;
        --BENCHES=*)
            BENCHES="${1#*=}"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [ -z "$LIBCS" ]; then
    LIBCS="glibc BUDAlloc ffmalloc markus"
fi

if [ -z "$THREADS" ]; then
    THREADS="1 2 4 8 16 32"
fi

function generate_benches() {
    local benches="parsec.blackscholes parsec.bodytrack parsec.canneal parsec.facesim parsec.ferret parsec.streamcluster parsec.fluidanimate parsec.freqmine parsec.netferret parsec.netstreamcluster parsec.swaptions parsec.vips parsec.x264"
    local libc="$1"
    local thread="$2"

    # Markus causes infinite loop in dedup and netdedup
    # So, pass these tests
    if [ $1 != "markus" ]; then
        benches+=" parsec.dedup parsec.netdedup"
    fi

    echo $benches
}

for LIBC in $LIBCS
do  
    for THREAD in $THREADS
    do
        if [ -z "$BENCHES" ]; then
            BENCHES=$(generate_benches $LIBC $THREAD)
        fi
        sudo rm -rf ../result/parsec/*.result ../result/parsec/*.time.out*
        sudo ./bench_parsec.sh --label="${LIBC}-${THREAD}t" --target=${LIBC} --size="native" --benchmarks="${BENCHES}" --threads=${THREAD}
        sudo ps -ef | grep parsec | grep -v bench_parsec_threads.sh | awk {'print $2'} | xargs -r sudo kill -9
    done
done