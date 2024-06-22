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
    echo "You are trying to run SPECCPU_2017 on a dangzero kernel with a non-dangzero library(s): $LIBCS."
    read -p "This will generate incorrect results. Are you sure? [y/n] "
    if [[ ! $REPLY =~ ^[Yy]$ ]]
    then
        exit 1
    fi
fi

BENCH_INT="600.perlbench_s 602.gcc_s 605.mcf_s 620.omnetpp_s 623.xalancbmk_s 625.x264_s 631.deepsjeng_s 641.leela_s 648.exchange2_s 657.xz_s"
BENCH_FLOAT="603.bwaves_s 607.cactuBSSN_s 619.lbm_s 621.wrf_s 628.pop2_s 638.imagick_s 644.nab_s 649.fotonik3d_s 654.roms_s"


if [[ $LIBCS = *"dangzero"* ]]; then
    SPEC_FOLDER="/trusted/SPECCPU_2017"
else
    SPEC_FOLDER="/home/$(cut -d/ -f3 <<< "$(realpath $(dirname "$0"))")/SPECCPU_2017"
fi

if [ ! -d "${SPEC_FOLDER}" ]; then
    echo "SPECCPU_2017 not exist!"
    exit 1
fi

for LIBC in $LIBCS
do
    sudo ../common/bench_spec.sh --label="${LIBC}" --target="${LIBC}" --threads="${THREADS}" --bench="SPECCPU_2017" --benchmarks="${BENCH_INT}" --result_label="INT" --taskset="${TASKSET}"
    sudo ../common/bench_spec.sh --label="${LIBC}" --target="${LIBC}" --threads="${THREADS}" --bench="SPECCPU_2017" --benchmarks="${BENCH_FLOAT}" --result_label="FLOAT" --taskset="${TASKSET}"
done
