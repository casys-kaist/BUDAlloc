#!/bin/bash

# Script to monitor Apache2 memory usage (RSS) every second for 600 seconds

# Define the output file
SCRIPT_DIR=$(realpath $(dirname "$0"))

if [ ! -d "$SCRIPT_DIR/../result/$1" ]; then
    mkdir -p $SCRIPT_DIR/../result/$1
fi

output_file="$SCRIPT_DIR/../result/$1/${1}_memory_usage.csv";

# Write the header to the output file
echo "Timestamp, Max RSS (KB)" > "$output_file"

sleep 5
# Start the monitoring loop
while true; do
    # Count the number of processes
    if [ "$1" == "apache2" ]; then
        count=$(ps -C httpd --no-headers | wc -l)
    else
        count=$(ps -C nginx --no-headers | wc -l)
    fi

    # Check if the count is zero
    if [ "$count" -eq 0 ]; then
        echo "Monitoring complete. Data saved to $output_file"
        exit
    fi

    timestamp=$(date +"%Y-%m-%d %H:%M:%S")
    if [ "$1" == "apache2" ]; then
        max_rss=$(ps -C httpd -o rss= | awk '{sum+=$1} END {print sum}')
    else
        max_rss=$(ps -C nginx -o rss= | awk '{sum+=$1} END {print sum}')
    fi

    # Log the data to the output file
    echo "$timestamp, $max_rss" >> "$output_file"

    # Wait for 1 second
    sleep 1
done

