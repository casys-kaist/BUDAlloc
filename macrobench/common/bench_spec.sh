#!/bin/bash

if [ "$#" -lt 5 ]; then
    echo "Usage: $0 [--label=value] [--target=value] [--threads=value] [--bench=value] [--benchmarks=value] [OPTIONS]"
    exit 1
fi

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--label=value] [--target=value] [--threads=value] [--bench=value] [--benchmarks=value] [OPTIONS]"
            exit 0
            ;;
        --label=*)
            LABEL="${1#*=}"
            shift
            ;;
        --target=*)
            export BENCH_TARGET="${1#*=}"
            shift
            ;;
        --iter=*)
            ITER="${1#*=}"
            shift
            ;;
        --threads=*)
            THREADS="${1#*=}"
            shift
            ;;
        --bench=*)
            BENCHES="${1#*=}"
            shift
            ;;
        --benchmarks=*)
            BENCHMARKS="${1#*=}"
            shift
            ;;

        # OPTIONAL from here
        --out_folder=*)
            OUT_FOLDER="${1#*=}"
            shift
            ;;
        --result_label=*)
            RESULT_LABEL="${1#*=}"
            shift
            ;;
        --taskset=*)
            TASKSET="${1#*=}"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

HOME=$(realpath $(dirname "$0"))

function main {
    if [ "$WEBHOOK" == "true" ]; then
        TSID=$($(realpath $(dirname "$0"))/bench-start-hook.sh --label="$LABEL" --type="${BENCHES}" --benchmarks="$BENCHMARKS")
    fi

    # Capture start time (in seconds since the epoch)
    start_time=$(date +%s)

    if [ ! -d "${HOME}/../result/${BENCHES}" ]; then
        mkdir -p ${HOME}/../result/${BENCHES}
    else 
        rm -rf ${HOME}/../result/${BENCHES}/*.result ${HOME}/../result/${BENCHES}/*.time.out*
    fi


    if [ "$LABEL" == "dangzero" ]; then
        DIR_SPEC="/trusted/${BENCHES}" #for dangzero
    else
        DIR_SPEC="/home/$(cut -d/ -f3 <<< "$HOME")/${BENCHES}"
    fi
    DIR_CONFIG="$HOME"

    if [ -z $OUT_FOLDER ]; then
        export DIR_RESULT=${DIR_CONFIG}/../result/${BENCHES}
    else
        export DIR_RESULT=${OUT_FOLDER}
    fi

    output_csv="$DIR_RESULT/result_${LABEL}_${RESULT_LABEL}.csv"

    echo "benchmark_name, max_rss, elapsed_time, speclog_path" > $output_csv
    mkdir -p /tmp/spec

    if [ "$BENCHES" == "SPECCPU_2006" ]; then
        cp ${DIR_CONFIG}/../common/eval.sh /tmp/spec/eval-2006.sh
    else
        cp ${DIR_CONFIG}/../common/eval.sh /tmp/spec/eval-2017.sh
    fi

    # Lauch spec
    cd $DIR_SPEC
    source $DIR_SPEC/shrc

    ## Required Setup for benchmark
    #Get current stack size limit
    current_limit=$(ulimit -s)

    # Check if the limit is unlimited or greater than or equal to 122880
    if [ "$current_limit" != "unlimited" ] && [ "$current_limit" -lt 122880 ]; then
        # Set the stack size limit to 122880
        ulimit -s 122880
        echo "Stack size limit updated to 122880"
    fi

    ### End of setup
    BENCHMARK_RUNNING_PATTERN="Running Benchmarks"
    SUCCESS=""
    FAIL=""
    UNKNOWN=""

    BENCH_LEFT=$(wc -w <<< "$BENCHMARKS")
    if [ ! -z $TASKSET ]; then
        PREFIX="taskset -c $TASKSET"
    else
        PREFIX="taskset -c 19"
    fi

    for BENCH in $BENCHMARKS; do
        result_path="$DIR_RESULT/$BENCH.result"
        if [ "$BENCHES" == "SPECCPU_2006" ]; then
            $PREFIX runspec --iterations 1 --size "ref" --action onlyrun --config  $DIR_CONFIG/../spec2006/spec_config.cfg --noreportable $BENCH | tee $result_path
        else 
            $PREFIX runcpu --action onlyrun --iterations 1 --size ref --threads $THREADS --config=$DIR_CONFIG/../spec2017/spec_config.cfg $BENCH | tee $result_path
        fi

        max_rss=0
        elapsed_time=0
        if grep -q "$BENCHMARK_RUNNING_PATTERN" "$result_path"; then
            if tail -n 15 "$result_path" | grep -q "Error"; then
                FAIL+="$BENCH "
                max_rss="failed"
                elapsed_time="failed"
            elif grep -q "Run Complete" "$result_path"; then
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
                speclog_path=$(grep "The log for this run is in" $result_path | awk '{print $8}')
                elapsed_time=$(tail -n 20 $speclog_path | grep "runtime=" | awk -F'=' '{print $3}' | cut -d ',' -f1)
            else
                UNKNOWN+="$BENCH "
                max_rss="unknown error"
                elapsed_time="unknown error"
            fi
        fi

        # Format: benchmark_name, max_rss, elapsed_time
        echo "$BENCH, $max_rss, $elapsed_time, $speclog_path" >> "$output_csv"

        if [ "$WEBHOOK" == "true" ]; then
            BENCH_LEFT=$(( $BENCH_LEFT - 1))
            $DIR_CONFIG/../common/bench-progress-hook.sh --tsid="$TSID" --benchmark="$BENCH" --left="$BENCH_LEFT"
        fi
    done

    echo "Benchmark success: $SUCCESS"
    echo "Benchmark fail: $FAIL"
    echo "Benchmark unknown: $UNKNOWN"


    if [ "$WEBHOOK" == "true" ]; then
        $DIR_CONFIG/../common/upload-webhook.sh --result-path="$output_csv" \
            --label="$LABEL" --type="${BENCHES}" --success="$SUCCESS" \
            --fail="$FAIL" --unknown="$UNKNOWN" --start-time="$start_time"
    fi
}

source $HOME/../environment
if [ "$USELOCK" == "true" ]; then
    IS_QUEUED="false"
    lockfile=/tmp/bench.lock
    timeout=86400 # 6hours timeout
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
                    ../common/bench-queued-hook.sh --label="$LABEL" --type="${BENCHES}" --benchmarks="$BENCHMARKS"
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