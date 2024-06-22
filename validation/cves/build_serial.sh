#!/bin/bash

# Define the starting directory (current directory by default)
START_DIR="."

# Find all install.sh scripts and run them
find "$START_DIR" -type f -name "install.sh" -print0 | while IFS= read -r -d '' script; do
    script_dir=$(dirname "$script")
    echo "Running $script in $script_dir"
    (cd "$script_dir" && bash "./install.sh")
done