#!/bin/bash

# Change variables to match your environment
if [ $SUDO_USER ]; then
    USER_NAME="$SUDO_USER"
else
    USER_NAME="$(whoami)"
fi
# PERF_PATH="/home/babamba/sBPF-Kernel/tools/perf/perf"
PERF_PATH=perf

########### Main Script #############
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
RESULT_DIR="${SCRIPT_DIR}/result"
FLAME_GRAPH="${SCRIPT_DIR}/flamegraph"
# IMAGE=$(date +%m%d_%H:%M:%S)

# Check sudo
if [ "$EUID" -ne 0 ]
then 
    echo "[Error] Please run script with sudo"
    exit 1
fi

# Make result folder
if [ ! -d $RESULT_DIR ]; then
    mkdir -p $RESULT_DIR
    sudo chown ${USER_NAME}.${USER_NAME} $RESULT_DIR
fi

# Separate by input type
if [[ $1 == "-e" ]]  # if it is executable
then
    # Check arguments
    if [[ $2 == "" ]] 
    then
        echo "Usage: sudo $0 -e [executable path / command]"
        exit 1
    fi

    # Extract command
    shift
    COMM="$*"

    # Create FIFO pipe
    PIPE=/tmp/perf
    if [ -p "$PIPE" ]; then
        rm "$PIPE"
    fi
    mkfifo "$PIPE"

    # Suspend target process
    export LD_PRELOAD=libkernel.so
    # export LD_PRELOAD=
    $COMM < "$PIPE" &
    PID=$!
    export LD_PRELOAD=
    echo "Program PID: $PID"
    
    # Start perf record
    ${PERF_PATH} record -F 499 -ag -p $PID > /dev/null &
    PERF_PID=$!
    sleep 1

    # Start target process and perf record
    echo "start" > "$PIPE"

    # Wait until both processes are finished
    wait $PID
    wait $PERF_PID
    rm "$PIPE"

    # Make perf script
    ${PERF_PATH} script > result/out.perf 2>/dev/null
    chown ${USER_NAME}.${USER_NAME} result/out.perf
    OUT_PERF_PATH=$RESULT_DIR/out.perf

elif [[ $1 == "-f" ]] # if it is perf script
then
    # Check arguments
    if [[ $2 == "" ]] 
    then
        echo "Usage: sudo $0 -f [perf script path]"
        exit 1
    fi
    OUT_PERF_PATH=$(realpath "$2")

else
    echo "Usage: sudo $0 (-f/-e) [path]"
    exit 1
fi

# Perf log classifier
python $SCRIPT_DIR/python_helper/classify_func.py -f ${OUT_PERF_PATH} > result/classification_result.txt
chown ${USER_NAME}.${USER_NAME} $RESULT_DIR/classification_result.txt

# Make flamegraph
$FLAME_GRAPH/stackcollapse-perf.pl ${OUT_PERF_PATH} > out.perf-folded
$FLAME_GRAPH/flamegraph.pl out.perf-folded > $RESULT_DIR/graph.svg
chown ${USER_NAME}.${USER_NAME} $RESULT_DIR/graph.svg

# Delete unnecessary
FILES=(
    $RESULT_DIR/out.perf-folded
    $RESULT_DIR/../perf.data
    $RESULT_DIR/out.perf.old
)

for FILE in "${FILES[@]}"; do
    if [ -f "$FILE" ]; then
        rm $FILE
    fi
done
