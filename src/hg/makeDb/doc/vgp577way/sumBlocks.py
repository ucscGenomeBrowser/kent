#!/usr/bin/env python3

import sys

def sumByIdentifier(inputFile):
    sums = {}
    
    with open(inputFile, 'r') as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) == 2:
                value = int(parts[0])
                identifier = parts[1]
                
                if identifier in sums:
                    sums[identifier] += value
                else:
                    sums[identifier] = value
    
    for identifier, total in sums.items():
        print(f"{total}\t{identifier}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: script.py input_file > output_file")
        sys.exit(1)
    
    inputFile = sys.argv[1]
    sumByIdentifier(inputFile)
