#!/bin/bash
THREADS="1"

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--LIBCS=value] [--TASKSET=value]"
            exit 0
            ;;
        --LIBCS=*)
            LIBCS="${1#*=}"
            shift
            ;;
        --TASKSET=*)
            TASKSET="${1#*=}"
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

if [ -z "$TASKSET" ]; then
    TASKSET=19;
fi

if [[ $(uname -r) == "4.0.0-kml" ]] && [[ $LIBCS != "dangzero" ]]; then
    echo "You are trying to run SPECCPU_2006 on a dangzero kernel with a non-dangzero library(s): $LIBCS."
    read -p "This will generate incorrect results. Are you sure? [y/n] "
    if [[ ! $REPLY =~ ^[Yy]$ ]]
    then
        exit 1
    fi
fi

BENCH_INT="400.perlbench 401.bzip2 403.gcc 429.mcf 445.gobmk 456.hmmer 458.sjeng 462.libquantum 464.h264ref 471.omnetpp 473.astar 483.xalancbmk"
BENCH_FLOAT="433.milc 444.namd 447.dealII 450.soplex 453.povray 470.lbm 482.sphinx3"


if [[ $LIBCS == *"dangzero"* ]]; then
    SPEC_FOLDER="/trusted/SPECCPU_2006"
else
    SPEC_FOLDER="/home/$(cut -d/ -f3 <<< "$(realpath $(dirname "$0"))")/SPECCPU_2006"
fi

if [ ! -d "${SPEC_FOLDER}" ]; then
    echo "SPECCPU_2006 not exist!"
    exit 1
fi

for LIBC in $LIBCS
do
    sudo ../common/bench_spec.sh --label="${LIBC}" --target="${LIBC}" --threads="${THREADS}" --bench="SPECCPU_2006" --benchmarks="${BENCH_INT}" --result_label="INT" --taskset="${TASKSET}"
    sudo ../common/bench_spec.sh --label="${LIBC}" --target="${LIBC}" --threads="${THREADS}" --bench="SPECCPU_2006" --benchmarks="${BENCH_FLOAT}" --result_label="FLOAT" --taskset="${TASKSET}"
done