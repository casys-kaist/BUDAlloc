#!/bin/bash

source $(realpath $(dirname "$0"))/../environment
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Usage: $0 [--result-path=value] [--label=value] [--type=value] [--success=value] [--fail=value] [--unknown=value] [--start-time=value]"
            exit 0
            ;;
        --result-path=*)
            RESULT_CSV="${1#*=}"
            shift
            ;;
        --label=*)
            LABEL="${1#*=}"
            shift
            ;;
        --type=*)
            TYPE="${1#*=}"
            shift
            ;;
        --success=*)
            SUCCESS="${1#*=}"
            shift
            ;;
        --fail=*)
            FAIL="${1#*=}"
            shift
            ;;
        --unknown=*)
            UNKNOWN="${1#*=}"
            shift
            ;;
        --start-time=*)
            start_time="${1#*=}"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

### Add new sheet of result in google sheet ###
base64var() {
    printf "$1" | base64stream
}

base64stream() {
    base64 | tr '/+' '_-' | tr -d '=\n'
}

# Header
header="{\"alg\":\"RS256\",\"typ\":\"JWT\",\"kid\":\"$GOOGLE_SERVICE_ACCOUNT_KID\"}"

# Payload
iat=$(date +%s)
exp=$(($iat + 3600))
payload="{\"iss\": \"$GOOGLE_SERVICE_ACCOUNT_EMAIL\",
    \"scope\": \"https://www.googleapis.com/auth/drive\",
    \"aud\": \"https://oauth2.googleapis.com/token\",
    \"exp\": \"$exp\",
    \"iat\": \"$iat\"}"

# Signature
request_body="$(base64var "$header").$(base64var "$payload")"
signature=$(openssl dgst -sha256 -sign <(echo -en $PEMKEY) <(printf "$request_body") | base64stream)

# JWT
jwt="$request_body.$signature"

oauth2_token=$(curl -s -X POST https://oauth2.googleapis.com/token \
    --data-urlencode 'grant_type=urn:ietf:params:oauth:grant-type:jwt-bearer' \
    --data-urlencode "assertion=$jwt" | grep -o '"access_token":"[^"]*' | awk -F '"' '{print $4}')

report_time=$(TZ='Asia/Seoul' date +"%Y-%m-%d %H:%M:%S")
sheet_id=$(shuf -i 1-999999999 -n 1)
csv_data=$(cat $RESULT_CSV)
output=$(curl -s --http1.1 -X POST -H "Authorization: Bearer $oauth2_token" \
    -H 'Content-type: application/json' \
    --data "{
  \"requests\": [
    {
        \"addSheet\": {
            \"properties\": {
                \"sheetId\": $sheet_id,
                \"title\": \"$TYPE-$LABEL-$report_time\"
            }
        }
    },
    {
        \"pasteData\": {
            \"coordinate\": {
                \"sheetId\": $sheet_id,
                \"rowIndex\": 0,
                \"columnIndex\": 0
            },
            \"data\": \"$csv_data\",
            \"type\": \"PASTE_NO_BORDERS\",
            \"delimiter\": \",\"
        }
    }
  ]
}" "https://sheets.googleapis.com/v4/spreadsheets/$GOOGLE_SHEET_ID:batchUpdate")

# Capture end time
end_time=$(date +%s)

# Calculate the difference in seconds
elapsed_seconds=$((end_time - start_time))

# Convert seconds to HH:MM:SS format
hours=$(printf "%02d" $((elapsed_seconds / 3600)))
minutes=$(printf "%02d" $(( (elapsed_seconds % 3600) / 60 )))
seconds=$(printf "%02d" $((elapsed_seconds % 60)))

curl -s -X POST -H 'Content-type: application/json' \
    --data "{
	\"blocks\": [
		{
			\"type\": \"header\",
			\"text\": {
				\"type\": \"plain_text\",
				\"text\": \"[🟢$TYPE🟢] Complete\",
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
					\"text\": \"*Total Time Elapsed:*\n$hours:$minutes:$seconds\"
				}
			]
		},
		{
			\"type\": \"section\",
			\"fields\": [
				{
					\"type\": \"mrkdwn\",
					\"text\": \"*Success:*\n$SUCCESS\"
				}
			]
		},
		{
			\"type\": \"section\",
			\"fields\": [
				{
					\"type\": \"mrkdwn\",
					\"text\": \"*Fail:*\n$FAIL\"
				}
			]
		},
		{
			\"type\": \"section\",
			\"fields\": [
				{
					\"type\": \"mrkdwn\",
					\"text\": \"*Unknown:*\n$UNKNOWN\"
				}
			]
		},
		{
			\"type\": \"section\",
			\"text\": {
				\"type\": \"mrkdwn\",
				\"text\": \"<https://docs.google.com/spreadsheets/d/$GOOGLE_SHEET_ID/edit#gid=$sheet_id|View result in Google Sheet>\"
			}
		}
	]
}" $SLACK_WEBHOOK_URL
