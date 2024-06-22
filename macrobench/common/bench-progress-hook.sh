#!/bin/bash

source $(realpath $(dirname "$0"))/../environment
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--tsid=value] [--benchmark=value] [--left=value]"
            exit 0
            ;;
        --tsid=*)
            TSID="${1#*=}"
            shift
            ;;
        --benchmark=*)
            BENCH="${1#*=}"
            shift
            ;;
        --left=*)
            LEFT="${1#*=}"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

CURRENT=$(TZ='Asia/Seoul' date '+%Y-%m-%d %H:%M:%S')

curl -s -X POST \
	-H "Authorization: Bearer $BOT_TOKEN" \
	-H 'Content-Type: application/json;charset=utf-8' \
    --data "{
	\"channel\": \"$CHANNEL_ID\",
	\"thread_ts\": \"$TSID\",
	\"text\": \"$BENCH done. $LEFT benchmarks left.\"
}" $SLACK_POSTMESSAGE_URL