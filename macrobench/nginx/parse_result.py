#!/bin/python3
import re
import sys
import argparse

def process_text_to_csv(input_text):
    # Regular expression pattern to capture relevant information
    connections_pattern = r"(\d+) connections"
    latency_pattern = r"Latency\s+([\d.]+ms|[\d.]+s|[\d.]+us)\s+([\d.]+ms|[\d.]+s|[\d.]+us)\s+([\d.]+ms|[\d.]+s|[\d.]+us)"
    rps_pattern = r"Requests/sec:\s+([\d.]+)"
    
    # Split the input text into benchmark blocks
    benchmark_blocks = re.split(r"[\n\r]Running", input_text)

    # Prepare CSV content
    csv_lines = ["Connections,Latency Avg,Latency Stdev,Latency Max,Requests/sec"]

    # Find all matches, using re.DOTALL to match across multiple lines
    for block in benchmark_blocks:
        connections_match = re.search(connections_pattern, block)
        latency_match = re.search(latency_pattern, block)
        rps_match = re.search(rps_pattern, block)

        if connections_match and latency_match and rps_match:
            connections = connections_match.group(1)
            latency_avg, latency_stdev, latency_max = latency_match.groups()
            rps = rps_match.group(1)
            csv_line = f"{connections},{latency_avg},{latency_stdev},{latency_max},{rps}"
            csv_lines.append(csv_line)
        # else:
        #     print(block, connections_match, latency_match, rps_match)

    return "\n".join(csv_lines)

# parse args
parser = argparse.ArgumentParser()
parser.add_argument('library', type=str, help='library name')
args = parser.parse_args()
lib = args.library

# Read input from stdin
input_text = sys.stdin.read()

# Process the input text and get CSV content
csv_content = process_text_to_csv(input_text)

# Write to CSV file
with open("../result/nginx/results_{}.csv".format(lib), "w+") as file:
    file.write(csv_content)

print("CSV file created successfully. (results.csv)")
