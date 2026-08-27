"""
Given a number of days and a  metadata file with sample names in the first column and sample collection dates in a
'date' column, write a file of sample names whose collection date is in the past n days.
"""

import argparse
from datetime import datetime
import gzip
import sys


def days_ago(date: str) -> int:
    """
    If date is well-formed, return its number of days ago.  Otherwise return a large int.
    """
    try:
        time_difference = datetime.now() - datetime.strptime(date, '%Y-%m-%d')
        return time_difference.days
    except:
        try:
            time_difference = datetime.now() - datetime.strptime(date, '%Y-%m')
            return time_difference.days
        except:
            try:
                time_difference = datetime.now() - datetime.strptime(date, '%Y')
                return time_difference.days
            except:
                return sys.maxsize


def find_recent_samples(input_metadata_file: str, n: int, output_file: str):
    """
    Scan metadata file for samples collected within the past n days, write those sample names to output.
    """
    with gzip.open(input_metadata_file, 'rt') if input_metadata_file.endswith('.gz') else open(input_metadata_file, 'r') as fin:
        with open(output_file, 'w') as fout:
            header = [word.strip() for word in fin.readline().split('\t')]
            name_ix = 0
            try:
                date_ix = header.index('date')
            except:
                print(f"Error: header of {input_metadata_file} does not have a 'date' column (values: {', '.join(header)})", file=sys.stderr)
                sys.exit(1)
            print(f"Using first column '{header[name_ix]}' for sample name and column '{header[date_ix]}' " +
                  "for sample collection date", file=sys.stderr)
            for line in fin:
                words = line.split('\t')
                name = words[name_ix].strip()
                date = words[date_ix].strip()
                if days_ago(date) <= n:
                    print(name, file=fout)


def main():
    parser = argparse.ArgumentParser(description="Extract sample names whose collection date is within the past N days.")
    parser.add_argument('-i', '--input-metadata', required=True,
                        help="Input metadata file, tab-separated, with sample name in the first column and sample " +
                        "collection date in a column labeled 'date'")
    parser.add_argument('-n', '--number-of-days', required=True, type=int,
                        help="Include sample name in output if its collection date is within the past N days")
    parser.add_argument('-o', '--output', required=True,
                        help="Output text file, with one sample name per line of samples collected within the past " +
                        "N days")
    args = parser.parse_args()
    find_recent_samples(args.input_metadata, args.number_of_days, args.output)


if __name__ == "__main__":
    main()
