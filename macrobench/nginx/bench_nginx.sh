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
    NUM_CONNECTIONS="400"
fi

if [ -z "$NUM_THREADS" ]; then
    NUM_THREADS=16
fi

if [ -z "$BENCH_SEC" ]; then
    BENCH_SEC=30
fi

if [[ $(uname -r) == "4.0.0-kml" ]] && [[ $LIBCS != "dangzero" ]]; then
    echo "You are trying to run nginx on a dangzero kernel with a non-dangzero library(s): $LIBCS."
    read -p "This will generate incorrect results. Are you sure? [y/n] "
    if [[ ! $REPLY =~ ^[Yy]$ ]]
    then
        exit 1
    fi
fi

if [ ! -d "../result/nginx" ]; then
    mkdir -p "../result/nginx"
fi

# SERVER CONFIGURATION
source ../server_conf
SERVER_COMMAND="cd BUDAlloc-User/macrobench/common; ./run_apache_nginx_client.sh nginx \"${NUM_CONNECTIONS}\" ${NUM_THREADS} ${BENCH_SEC}"

for LIBC in $LIBCS
do
    # make pipe
    PIPE=/tmp/wait
    if [ -p "$PIPE" ]; then
        rm "$PIPE"
    fi
    mkfifo "$PIPE"

    # Run nginx
    ../common/run_apache_nginx_server.sh nginx $LIBC $NUM_THREADS "background" &

    # Make Command
    SERVER_COMMAND+=" ${LIBC}"

    # Wait until nginx is ready
    cat $PIPE

    # Run benchmark and parse result
    results=$(sshpass -p ${USER_PW} ssh -o StrictHostKeyChecking=no ${USER_NAME}@${NET_IP_CLIENT} ${SERVER_COMMAND})
    echo "$results" | ./parse_result.py $LIBC

    # Signal to nginx
    echo "bench finished" > "$PIPE"

    # Wait until clean finished
    cat < "$PIPE" > /dev/null

    # kill still running processes
    ps -ef | grep run_apache_nginx.sh | awk {'print $2'} | xargs -r sudo kill -9
    ps -ef | grep nginx | grep process | awk {'print $2'} | xargs -r sudo kill -9

    # Parse memory log
    MAX_RSS_FILE="../result/nginx/nginx_memory_usage_${LIBC}.csv"
    echo "Connections, Timestamp, Max_rss" > $MAX_RSS_FILE
    DATA=$(cat ../result/nginx/nginx_memory_usage.csv | ./parse_memory.py)
    echo "${NUM_CONNECTIONS}, ${DATA}" >> $MAX_RSS_FILE
done
