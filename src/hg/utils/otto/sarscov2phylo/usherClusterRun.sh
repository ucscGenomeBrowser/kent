#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/usherClusterRun.sh

usage() {
    echo "usage: $0 today"
}

if [ $# != 1 ]; then
  usage
  exit 1
fi

today=$1

ottoDir=/hive/data/outside/otto/sarscov2phylo
scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

usherDir=~angie/github/usher
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

cd $ottoDir/$today

# 16 cluster jobs, 16 threads each --> 1/4 capacity of ku cluster.  (for many hours)
jobCount=16
threadCount=16

sampleCount=$(vcfSamples new.masked.vcf.gz | wc -l)
samplesPerJob=$(( ($sampleCount + ($jobCount-1)) / $jobCount ))

echo $sampleCount samples, $jobCount jobs, $samplesPerJob samples per job
for ((i=0;  $i < $jobCount;  i++)); do
    startIx=$(( 10 + ($i * $samplesPerJob) ))
    if [ $i == $(( $jobCount - 1 )) ]; then
        endIx=$(( 9 + $sampleCount ))
    else
        endIx=$(( 9 + (($i+1) * $samplesPerJob) ))
    fi
    echo job $i, genotype columns $startIx-$endIx
    zcat new.masked.vcf.gz \
    | cut -f 1-9,$startIx-$endIx \
    | gzip -c \
        > new.$i.masked.vcf.gz
    vcfSamples new.$i.masked.vcf.gz | wc -l
done

cat > runUsher.sh <<EOF
#!/bin/bash
set -beEu
vcf=\$1
pbIn=\$2
pbOut=\$3
log=\$4
/cluster/home/angie/github/usher/build/usher -u -T $threadCount -A -e 5 --max-parsimony-per-sample 20 -v \$vcf -i \$pbIn -o \$pbOut >& \$log
EOF
chmod a+x runUsher.sh

cp /dev/null jobList
for ((i=0;  $i < $jobCount;  i++)); do
    echo "./runUsher.sh new.$i.masked.vcf.gz prevRenamed.pb {check out exists preTrim.$i.pb} usher.addNew.$i.log" >> jobList
done

ssh ku "cd $ottoDir/$today && para -cpu=$threadCount -priority=10 make jobList"

# Make a combined log (updateCombinedTree.sh looks there for sequences with excessively high EPPs)
cat usher.addNew.*.log > usher.addNew.log

# Merge 16 protobufs into 1
time $matUtils merge -T 8 --input-mat-1 preTrim.0.pb --input-mat-2 preTrim.1.pb -o merged.0.1.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.2.pb --input-mat-2 preTrim.3.pb -o merged.2.3.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.4.pb --input-mat-2 preTrim.5.pb -o merged.4.5.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.6.pb --input-mat-2 preTrim.7.pb -o merged.6.7.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.8.pb --input-mat-2 preTrim.9.pb -o merged.8.9.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.10.pb --input-mat-2 preTrim.11.pb -o merged.10.11.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.12.pb --input-mat-2 preTrim.13.pb -o merged.12.13.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.14.pb --input-mat-2 preTrim.15.pb -o merged.14.15.pb &
wait

time $matUtils merge -T 16 --input-mat-1 merged.0.1.pb --input-mat-2 merged.2.3.pb -o merged.0.3.pb &
$matUtils merge -T 16 --input-mat-1 merged.4.5.pb --input-mat-2 merged.6.7.pb -o merged.4.7.pb &
$matUtils merge -T 16 --input-mat-1 merged.8.9.pb --input-mat-2 merged.10.11.pb -o merged.8.11.pb &
$matUtils merge -T 16 --input-mat-1 merged.12.13.pb --input-mat-2 merged.14.15.pb -o merged.12.15.pb &
wait

time $matUtils merge -T 32 --input-mat-1 merged.0.3.pb --input-mat-2 merged.4.7.pb -o merged.0.7.pb &
$matUtils merge -T 32 --input-mat-1 merged.8.11.pb --input-mat-2 merged.12.15.pb -o merged.8.15.pb &
wait

time $matUtils merge -T 64 --input-mat-1 merged.0.7.pb --input-mat-2 merged.8.15.pb -o merged.pb

# Optimize
$matOptimize -T 40 -r 8 -S move_log -i merged.pb -M 6 -o gisaidAndPublic.$today.masked.preTrim.pb \
    >& matOptimize.merged.log
