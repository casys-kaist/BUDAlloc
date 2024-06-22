#!/bin/bash

ACTION="run"

if [ "$#" -lt 5 ]; then
    echo "Usage: $0 [--label=value] [--target=value] [--size=value] [--benchmarks=value] [--threads=value]"
    exit 1
fi

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--label=value] [--target=value] [--size=value] [--benchmarks=value] [--threads=value]"
            exit 0
            ;;
        --label=*)
            LABEL="${1#*=}"
            shift
            ;;
        --target=*)
            BENCH_TARGET="${1#*=}"
            shift
            ;;
        --size=*)
            SIZE="${1#*=}"
            shift
            ;;
        --threads=*)
            NUM_THREADS="${1#*=}"
            shift
            ;;
        --benchmarks=*)
            BENCHES="${1#*=}"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

HOME=$(realpath $(dirname "$0"))
USER_NAME=$(cut -d/ -f3 <<< "$(realpath $0)")

convert_to_seconds() {
    local time=$1
    local hours=0
    local minutes=0
    local seconds=0.0

    # Count the number of colons in the time string
    local colon_count=$(grep -o ":" <<< "$time" | wc -l)

    if [ $colon_count -eq 2 ]; then
        # Format is hh:mm:ss.ss
        IFS=: read -r hours minutes seconds <<< "$time"
    elif [ $colon_count -eq 1 ]; then
        # Format is mm:ss.ss
        IFS=: read -r minutes seconds <<< "$time"
    else
        echo "Invalid time format"
        return 1
    fi

    # Convert to total seconds with double precision
    total_seconds=$(awk "BEGIN {printf \"%.2f\", ($hours * 3600) + ($minutes * 60) + $seconds}")
    echo $total_seconds
}

function main {
    if [ "$WEBHOOK" == "true" ]; then
        TSID=$(${HOME}/../common/bench-start-hook.sh --label="$LABEL" --type="PARSEC" --benchmarks="$BENCHES")
    fi

    # Capture start time (in seconds since the epoch)
    start_time=$(date +%s)

    if [ ! -d "${HOME}/../result/parsec" ]; then
        mkdir -p ${HOME}/../result/parsec
    fi

    export DIR_RESULT=$HOME/../result/parsec
    DIR_BACKUP="/home/${USER_NAME}/parsec-log/$(TZ="Asia/Seoul" date '+%Y-%m-%d(%H:%M:%S)')/"
    mkdir -p "$DIR_BACKUP"
    output_csv="$DIR_RESULT/result_${LABEL}.csv"
    echo "benchmark_name, max_rss, elapsed_time, result_path" > $output_csv
    cp ./eval.sh /tmp/eval-parsec.sh
    chmod +x /tmp/eval-parsec.sh

    if ! [ -x "$(command -v parsecmgmt)" ]; then
        if [ -d "parsec-benchmark" ]; then
            cd parsec-benchmark
            . env.sh
        else
            echo 'run install.sh first'
            exit 1
        fi
    fi

    PREFIX="taskset -c 1-${NUM_THREADS}"
    BENCHMARK_RUNNING_PATTERN="Running "
    SUCCESS=""
    FAIL=""
    UNKNOWN=""

    BENCH_LEFT=$(wc -w <<< "$BENCHES")
    for BENCH in $BENCHES; do
        result_path="$DIR_RESULT/$BENCH.result"
        SUDO_PREFIX="sudo DIR_RESULT=${DIR_RESULT} benchmark_name=${BENCH} BENCH_TARGET=${BENCH_TARGET} USER_NAME=${USER_NAME}"
        ${PREFIX} ${SUDO_PREFIX} ./bin/parsecmgmt -a "run" -n $NUM_THREADS -i $SIZE -s "/tmp/eval-parsec.sh" -p $BENCH | tee $result_path

        # Backup Run result log into other directory
        cp "$result_path" "$DIR_BACKUP$BENCH.result"

        max_rss=0
        elapsed_time=0
        if grep -q "$BENCHMARK_RUNNING_PATTERN" "$result_path"; then
            if grep -q "Command terminated" "$result_path" || grep -q "Command exited" "$result_path"; then
                FAIL+="$BENCH "
                max_rss="failed"
                elapsed_time="failed"
            elif grep -q "Exit status: 0" "$result_path"; then
                SUCCESS+="$BENCH "

                timeout_file_prefix="$DIR_RESULT/$BENCH.time.out"

                # Iter over files of time out and get the maximum RSS
                for timeout_file in $timeout_file_prefix*; do
                    if [ -f "$timeout_file" ]; then
                        rss=$(grep "Maximum resident set size" $timeout_file | awk '{print $6}')
                        if (( rss > max_rss )); then
                            max_rss=$rss
                        fi
                    fi
                done

                # Extract elapsed time from the output
                elapsed_time_str=$(grep "Elapsed (wall clock) time (h:mm:ss or m:ss):" $result_path | awk '{print $8}')
                elapsed_time=$(convert_to_seconds "$elapsed_time_str")
            else
                UNKNOWN+="$BENCH "
                max_rss="unknown error"
                elapsed_time="unknown error"
            fi
        fi

        # Format: benchmark_name, max_rss, elapsed_time
        echo "$BENCH, $max_rss, $elapsed_time, $DIR_BACKUP$BENCH.result" >> "$output_csv"
        
        if [ "$WEBHOOK" == "true" ]; then
            BENCH_LEFT=$(( $BENCH_LEFT - 1))
            $DIR_RESULT/../../common/bench-progress-hook.sh --tsid="$TSID" --benchmark="$BENCH" --left="$BENCH_LEFT"
        fi
    done

    echo "Benchmark success: $SUCCESS"
    echo "Benchmark fail: $FAIL"
    echo "Benchmark unknown: $UNKNOWN"

    if [ "$WEBHOOK" == "true" ]; then
        $DIR_RESULT/../../common/upload-webhook.sh --result-path="$output_csv" \
            --label="$LABEL" --type="PARSEC" --success="$SUCCESS" \
            --fail="$FAIL" --unknown="$UNKNOWN" --start-time="$start_time"
    fi
}

source $HOME/../environment
if [ "$USELOCK" == "true" ]; then
    IS_QUEUED="false"
    lockfile=/tmp/bench.lock
    timeout=21600 # 6hours timeout
    # Wait for the lock
    (
        end_time=$(( $(date +%s) + timeout ))
        while true; do
            if flock -n 200; then
                # Lock acquired, run commands
                echo "Lock acquired, running commands..."
                # Place your commands here
                main
                # Release the lock
                break
            else
                if [ "$IS_QUEUED" == "false" ]; then
                    IS_QUEUED="true"
                    ../common/bench-queued-hook.sh --label="$LABEL" --type="PARSEC" --benchmarks="$BENCHES"
                fi 
                echo "Lock is busy, waiting..."
                # Check if timeout is reached
                current_time=$(date +%s)
                if [ "$current_time" -ge "$end_time" ]; then
                    echo "Timeout reached, exiting."
                    exit 1
                fi
            fi
            sleep 60  # Wait for 60 seconds before retrying
        done
    ) 200>${lockfile}
else
    main
fi
