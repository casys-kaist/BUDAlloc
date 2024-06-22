#!/bin/bash

source $(realpath $(dirname "$0"))/../environment
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--label=value] [--type=value] [--benchmarks=value]"
            exit 0
            ;;
        --label=*)
            LABEL="${1#*=}"
            shift
            ;;
        --type=*)
            TYPE="${1#*=}"
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

CURRENT=$(TZ='Asia/Seoul' date '+%Y-%m-%d %H:%M:%S')

result=$(curl -s -X POST \
	-H "Authorization: Bearer $BOT_TOKEN" \
	-H 'Content-Type: application/json;charset=utf-8' \
    --data "{
	\"channel\": \"$CHANNEL_ID\",
	\"blocks\": [
		{
			\"type\": \"header\",
			\"text\": {
				\"type\": \"plain_text\",
				\"text\": \"[🚀$TYPE🚀] Starts\",
				\"emoji\": true
			}
		},
		{
			\"type\": \"section\",
			\"fields\": [
				{
					\"type\": \"mrkdwn\",
					\"text\": \"*Target:*\n$LABEL\"
				},
				{
					\"type\": \"mrkdwn\",
					\"text\": \"*Time Issued:*\n$CURRENT\"
				}
			]
		},
		{
			\"type\": \"section\",
			\"fields\": [
				{
					\"type\": \"mrkdwn\",
					\"text\": \"*Benchmarks:*\n$BENCHES\"
				}
			]
		}
	]
}" $SLACK_POSTMESSAGE_URL)

ts_value=$(echo $result | grep -o '"ts":"[^"]*' | sed -n '1s/"ts":"//p')
echo $ts_value
