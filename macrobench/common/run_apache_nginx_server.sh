#!/bin/bash

if [ "$#" -lt 2 ]; then
    echo "Usage : $0 <program: apache2/nginx> <library> <worker: default 16> <foreground/background>"
    exit 1
fi

if [ "$4" == "background" ]; then
    if [ ! -p "/tmp/wait" ]; then
        echo "You should run with "/tmp/wait" pipe enabled!"
        exit 1
    fi
fi

SCRIPT_DIR=$(realpath $(dirname "$0"))
FILE_SIZE=64
if [ "$1" == "apache2" ]; then
    FILE_PATH="$SCRIPT_DIR/../$1/${FILE_SIZE}bytes.txt"
    export UNSET_RTLD_DEEPBIND=1
elif [ "$1" == "nginx" ]; then
    FILE_PATH="/usr/local/nginx/html/${FILE_SIZE}bytes.txt"
else
    echo "Usage : $0 <program: apache2/nginx> <library> <worker: default 16> <foreground/background>"
    exit 1
fi

if [ -z "$3" ] || [ "$3" == "" ]; then
    WORKER=16
else
    WORKER=$3
fi

USER_NAME=$(cut -d/ -f3 <<< "$(realpath $0)")
HOME="/home/${USER_NAME}"

case "$2" in
    "glibc")
        PREFIX=""
        ;;
    "BUDAlloc")
        PREFIX="LD_PRELOAD=libkernel.so"
        ;;
    "ffmalloc")
        PREFIX="LD_PRELOAD=${HOME}/ffmalloc/libffmallocnpmt.so"
        ;;
    "markus")
        PREFIX="LD_PRELOAD=${HOME}/markus-allocator/lib/libgc.so"
        ;;
    "dangzero")
        PREFIX="LD_PRELOAD=/trusted/wrap.so"
        ;;
    *)
        echo "Usage : $0 <program: apache2/nginx> <library> <worker: default 16>"
        exit 1
        ;;
esac

sudo dd if=/dev/urandom of="$FILE_PATH" bs=1 count="$FILE_SIZE"
${SCRIPT_DIR}/track_memory.sh $1 &
TRACK_PID="$!"

if [ "$4" == "background" ]; then
    if [ "$2" == "dangzero" ]; then
        taskset -c 1-${WORKER} sudo ${PREFIX} /trusted/nginx/nginx -c $SCRIPT_DIR/../$1/nginx.conf -g "worker_processes $WORKER;" & # for dangzero
    elif [ "$1" == "apache2" ]; then
        taskset -c 1-${WORKER} sudo ${PREFIX} $SCRIPT_DIR/../$1/httpd/bin/httpd -D FOREGROUND -f $SCRIPT_DIR/../$1/httpd.conf &
    else
        taskset -c 1-${WORKER} sudo ${PREFIX} $SCRIPT_DIR/../$1/nginx-1.24.0/objs/nginx -c $SCRIPT_DIR/../$1/nginx.conf -g "worker_processes $WORKER;" &
    fi
else
    if [ "$2" == "dangzero" ]; then
        taskset -c 1-${WORKER} sudo ${PREFIX} /trusted/nginx/nginx -c $SCRIPT_DIR/../$1/nginx.conf -g "worker_processes $WORKER;" # for dangzero
    elif [ "$1" == "apache2" ]; then
        taskset -c 1-${WORKER} sudo ${PREFIX} $SCRIPT_DIR/../$1/httpd/bin/httpd -D FOREGROUND -f $SCRIPT_DIR/../$1/httpd.conf
    else
        taskset -c 1-${WORKER} sudo ${PREFIX} $SCRIPT_DIR/../$1/nginx-1.24.0/objs/nginx -c $SCRIPT_DIR/../$1/nginx.conf -g "worker_processes $WORKER;"
    fi
fi

if [ "$4" == "background" ]; then
    PID="$!"
    echo "$1 start" > "/tmp/wait"

    # Wait until worker finished
    cat < "/tmp/wait" > /dev/null

    sudo kill -9 $PID $TRACK_PID
    sudo pkill -x nginx

    # Signal to bench
    echo "clean finished" > "/tmp/wait"
fi