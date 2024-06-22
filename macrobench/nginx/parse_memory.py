#!/bin/python3
import sys

def find_max_rss(input_data):
    max_rss = 0
    max_rss_timestamp = ""

    # Process each line of the input data
    for line in input_data:
        parts = line.strip().split(',')
        if len(parts) == 2:
            timestamp, rss = parts[0], int(parts[1].strip())
            if rss > max_rss:
                max_rss = rss
                max_rss_timestamp = timestamp.strip()  # Assuming timestamp format is 'yyyy-mm-dd HH:MM:SS'
    
    return max_rss_timestamp, max_rss

def main():
    input_lines = sys.stdin.read().strip().split('\n')
    if input_lines[0].startswith("Timestamp"):  # Check for the header
        timestamp, max_rss = find_max_rss(input_lines[1:])  # Skip the header
    else:
        timestamp, max_rss = find_max_rss(input_lines)

    # Print the results with a header
    # print("Timestamp, Max RSS (KB)")
    print(f"{timestamp}, {max_rss}")

if __name__ == "__main__":
    main()
