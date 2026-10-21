#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

usage() {
    echo "usage: $0 lineageTree.pb prevDataRelease release"
}

if (( $# != 3 )); then
  usage
  exit 1
fi

lineageTreePb=$1
prevDataRelease=$2
release=$3

today=$(date +%F)

# Get absolute path to $lineageTreePb
if [[ ! -f $lineageTreePb ]]; then
    echo "Error: lineage tree file $lineageTreePb doesn't exist"
    exit 1
fi
lineageTreePbPath=$(cd "$(dirname "$lineageTreePb")" && pwd -P)/$(basename "$lineageTreePb")

cd $gisaidDir/chunks
threadCount=4
mkdir pangoCluster.v4.test.$today
cd pangoCluster.v4.test.$today

# Use /dev/shm for TMPDIR because /scratch/tmp was too slow
cat > run.sh <<EOF
#!/bin/bash
source ~/.bashrc
conda activate pangolin
set -beEu -x -o pipefail

export TMPDIR=/dev/shm

inFaXz=\$1
threadCount=\$2
outCsv=\$3

logfile=\$(mktemp)
linRep=\$(mktemp)
inFa=\$(mktemp)
xzcat \$inFaXz | sed -re 's@^>hCo[Vv]-19/+@>@;  s/[ '"'"',()]//g;' > \$inFa

pangolin \
    --usher-tree $lineageTreePbPath \
    --skip-designation-cache \
    --skip-scorpio \
    -t \$threadCount \
    \$inFa \
    --outfile \$linRep > \$logfile 2>&1
tail -n+2 \$linRep > \$outCsv
rm \$inFa \$linRep \$logfile
EOF
chmod a+x run.sh

for f in ../gisaid_epi_isl_*.fa.xz; do
    echo $(pwd)/run.sh $f $threadCount ../$(basename $f .fa.xz).pangoUsher.csv;
done > runAll.sh
wc -l runAll.sh

# Set job count to use 60% of available CPUs
maxRecentLoad=$(awk '{m=$1; if ($2>m) m=$2; if ($3>m) m=$3; print m}' /proc/loadavg)
availableCpus=$(echo $maxRecentLoad | awk '{print int('$(nproc)' - $1);}')
jobCount=$(echo $availableCpus | awk '{print int((0.6 * $1) / '$threadCount');}')

# Run the jobs
jobLog=$(mktemp)
echo "Running $jobCount jobs ($threadCount threads each), output going to $jobLog"
time parallel -j $jobCount < runAll.sh >& $jobLog
rm $jobLog

cd ..
awk -F, -v OFS=$'\t' '{print $1, $2, $8;}' gisaid_epi_isl_*.pangoUsher.csv \
    > pangolin.usher.test.$release.tsv
join -t$'\t' <(sort pangolin.usher.test.$prevDataRelease.tsv) \
             <(sort pangolin.usher.test.$release.tsv) \
    | tawk '$2 != $4 { print $1, $2, $4; }' \
    > pangolin.usher.diffs.latest
wc -l pangolin.usher.diffs.latest

cut -f 2,3 pangolin.usher.diffs.latest | sort | uniq -c | sort -nr | head -30
