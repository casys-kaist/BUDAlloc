#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run with sudo permission"
  exit
fi

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--LIBCS=value] [--CONNECTIONS=value] [--THREADS=value] [--BENCH_SEC=value]"
            exit 0
            ;;
        --LIBCS=*)
            LIBCS="${1#*=}"
            shift
            ;;
        --CONNECTIONS=*)
            NUM_CONNECTIONS="${1#*=}"
            shift
            ;;
        --THREADS=*)
            NUM_THREADS="${1#*=}"
            shift
            ;;
        --BENCH_SEC=*)
            BENCH_SEC="${1#*=}"
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

if [ -z "$NUM_CONNECTIONS" ]; then
    NUM_CONNECTIONS="100 200 400 800"
fi

if [ -z "$NUM_THREADS" ]; then
    NUM_THREADS=16
fi

if [ -z "$BENCH_SEC" ]; then
    BENCH_SEC=30
fi

# SERVER CONFIGURATION
source ../server_conf
rm -rf ./httpd/logs/*

if [ ! -d "../result/apache2" ]; then
    mkdir -p "../result/apache2"
fi

for LIBC in $LIBCS
do
    RESULT_FILE="../result/apache2/results_${LIBC}.csv"
    echo "Connections,Latency Avg,Latency Stdev,Latency Max,P50,P75,P90,P99,Requests/sec,timestamp" > $RESULT_FILE
    MAX_RSS_FILE="../result/apache2/apache2_memory_usage_${LIBC}.csv"
    echo "Connections, Timestamp, Max_rss" > $MAX_RSS_FILE

    for num_connection in $NUM_CONNECTIONS
    do  
        # make pipe
        PIPE=/tmp/wait
        if [ -p "$PIPE" ]; then
            rm "$PIPE"
        fi
        mkfifo "$PIPE"

        SERVER_COMMAND="cd BUDAlloc-User/macrobench/common; ./run_apache_nginx_client.sh apache2 ${num_connection} ${NUM_THREADS} ${BENCH_SEC} ${LIBC}"

        # Run apache2
        sudo ../common/run_apache_nginx_server.sh apache2 $LIBC $NUM_THREADS "background" &

        # Make Command
        SERVER_COMMAND+=" ${LIBC}"

        # Wait until apache2 is ready
        cat $PIPE

        # Run benchmark and parse result
        results=$(sshpass -p ${USER_PW} ssh -o StrictHostKeyChecking=no ${USER_NAME}@${NET_IP_CLIENT} ${SERVER_COMMAND})
        echo "$results"
        echo "$results" > "result.log"
        echo "$results" | ./parse_result.py $LIBC

        # Signal to apache2
        echo "bench finished" > "$PIPE"

        # Wait until clean finished
        cat < "$PIPE" > /dev/null

        # Kill still running httpd
        ps -ef | grep run_apache_nginx | awk {'print $2'} | xargs -r sudo kill -9
        ps -ef | grep httpd.conf | awk {'print $2'} | xargs -r sudo kill -9

        # Parse memory log
        DATA=$(cat ../result/apache2/apache2_memory_usage.csv | ./parse_memory.py)
        echo "${num_connection}, ${DATA}" >> $MAX_RSS_FILE

        # You should rest same as you work...
        echo "---------------------------------------------------------------"
        echo "Prepare next bench: sleep 1 min.."
        echo "---------------------------------------------------------------"
        sleep 60
    done
done
