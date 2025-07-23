#!/bin/bash

# Delete all .log and .txt files recursively from current directory
find . -type f \( -name "*.log" -o -name "*.txt" -o -name "output_mahimahi" \) -exec rm -f {} +

echo "✅ All .log and .txt files deleted."
