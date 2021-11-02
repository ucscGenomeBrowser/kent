#!/bin/bash
source ~/.bashrc
conda activate pangolin
set -beEu -o pipefail

fa=$1
out=$fa.pangolin.csv
logfile=$(mktemp)
pangolin $fa --outfile $out > $logfile 2>&1
rm $logfile
