#!/bin/bash
source ../server_conf

TEST_NAME=$1
NUM_CONNECTIONS=$2
NUM_THREADS=$3
BENCH_SEC=$4
LIBC=$5
FOLDER="../$TEST_NAME"

function main {
    if [ ! -d "../result/${TEST_NAME}" ]; then
        mkdir -p "../result/${TEST_NAME}"
    fi

    results=""
    for connections in $NUM_CONNECTIONS; do
        num_threads=$((connections >= NUM_THREADS ? NUM_THREADS : 1))

        if [ "$TEST_NAME" == "apache2" ]; then
            RESULT_FILE="../results.log"
            result="$(${FOLDER}/wrk/wrk -c $connections -d $BENCH_SEC --latency -t $num_threads http://${NET_IP_SERVER}/64bytes.txt)"$'\n'$"time: $(TZ=Asia/Seoul date +"%Y-%m-%d %H:%M:%S")"$'\n'
        else
            RESULT_FILE="../result/nginx/results_${LIBC}.log"
            result="$(${FOLDER}/wrk/wrk -c $connections -d $BENCH_SEC -t $NUM_THREADS http://${NET_IP_SERVER}/64bytes.txt)"$'\n'
        fi

        results+=$result
    done

    echo "$results"
    echo "$results" > ${RESULT_FILE}
    echo "$results" | ${FOLDER}/parse_result.py $LIBC
}

if [ "$TEST_NAME" == "apache2" ] || [ "$TEST_NAME" == "nginx" ]; then
    main
else
    echo "Usage : $0 <apache2/nginx> <NUM_CONNECTIONS> <NUM_THREADS> <BENCH_SEC> <LIBC>"
    exit 1
fi
