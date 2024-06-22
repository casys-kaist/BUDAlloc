#!/bin/bash

SPEC_FOLDER="/home/$(cut -d/ -f3 <<< "$(realpath $(dirname "$0"))")/SPECCPU_2006"

if [ ! -d "${SPEC_FOLDER}" ]; then
    echo "SPECCPU_2006 not exist!"
    exit 1
fi

THREADS="1"
LIBC="BUDAlloc"

BENCH_INT="400.perlbench 401.bzip2 403.gcc 429.mcf 445.gobmk 456.hmmer 458.sjeng 462.libquantum 464.h264ref 471.omnetpp 473.astar 483.xalancbmk"
BENCH_FLOAT="433.milc 444.namd 447.dealII 450.soplex 453.povray 470.lbm 482.sphinx3"

PREFIX="./func_classifier.sh -e ../../macrobench/common/bench_spec.sh"

for bench in $BENCH_INT
do
    sudo ${PREFIX} --label="${LIBC}" --target="${LIBC}" --threads="${THREADS}" --bench="SPECCPU_2006" --benchmarks="${bench}" --result_label="PERF_INT"
    sudo mv result/classification_result.txt result/classification_result_${bench}.txt
    sudo mv result/graph.svg result/graph_${bench}.svg
    sudo mv result/out.perf result/out_${bench}.perf
done

for bench in $BENCH_FLOAT
do
    sudo ${PREFIX} --label="${LIBC}" --target="${LIBC}" --threads="${THREADS}" --bench="SPECCPU_2006" --benchmarks="${bench}" --result_label="PERF_FLOAT"
    sudo mv result/classification_result.txt result/classification_result_${bench}.txt
    sudo mv result/graph.svg result/graph_${bench}.svg
    sudo mv result/out.perf result/out_${bench}.perf
done