#!/bin/bash
source ~/.bashrc
conda activate pangolin
set -beEu -x -o pipefail

# Run pangolin/pangoLEARN on a file (not pipe) and output full CSV
# (suitable for cluster run on faSplit sequence chunks)

fa=$1
out=$fa.pangolin.csv
logfile=$(mktemp)
pangolin --analysis-mode pangolearn $fa --outfile $out > $logfile 2>&1
rm $logfile
