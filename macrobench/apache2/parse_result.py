#!/bin/python3
import re
import sys, argparse
from datetime import datetime

def process_text_to_csv(input_text):
    # Regular expression pattern to capture relevant information
    connections_pattern = r"(\d+) connections"
    latency_pattern = r"Latency\s+([\d.]+ms|[\d.]+s|[\d.]+us)\s+([\d.]+ms|[\d.]+s|[\d.]+us)\s+([\d.]+ms|[\d.]+s|[\d.]+us)"
    l50_pattern = r"50%\s+([\d.]+ms|[\d.]+us|[\d.]+s)"
    l75_pattern = r"75%\s+([\d.]+ms|[\d.]+us|[\d.]+s)"
    l90_pattern = r"90%\s+([\d.]+ms|[\d.]+us|[\d.]+s)"
    l99_pattern = r"99%\s+([\d.]+ms|[\d.]+us|[\d.]+s)"
    rps_pattern = r"Requests/sec:\s+([\d.]+)"
    time_pattern = r"time:\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})"
    
    # Split the input text into benchmark blocks
    benchmark_blocks = re.split(r"[\n\r]Running", input_text)

    # Prepare CSV content
    # csv_lines = ["Connections,Latency Avg,Latency Stdev,Latency Max,P50,P75,P90,P99,Requests/sec,timestamp"]
    csv_lines = []

    # Find all matches, using re.DOTALL to match across multiple lines
    for block in benchmark_blocks:
        connections_match = re.search(connections_pattern, block)
        latency_match = re.search(latency_pattern, block)
        l50_match = re.search(l50_pattern, block)
        l75_match = re.search(l75_pattern, block)
        l90_match = re.search(l90_pattern, block)
        l99_match = re.search(l99_pattern, block)
        rps_match = re.search(rps_pattern, block)
        time_match = re.search(time_pattern, block)

        if connections_match and latency_match and rps_match:
            connections = connections_match.group(1)
            latency_avg, latency_stdev, latency_max = latency_match.groups()
            l50 = l50_match.group(1)
            l75 = l75_match.group(1)
            l90 = l90_match.group(1)
            l99 = l99_match.group(1)
            rps = rps_match.group(1)
            time = time_match.group(1)
            csv_line = f"{connections},{latency_avg},{latency_stdev},{latency_max},{l50},{l75},{l90},{l99},{rps},{time}"
            csv_lines.append(csv_line)
        # else:
        #     print(block, connections_match, latency_match, rps_match)

    return "\n".join(csv_lines) + "\n"

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
with open("../result/apache2/results_{}.csv".format(lib), "a+") as file:
    file.write(csv_content)

print("CSV file created successfully. (results.csv)")
