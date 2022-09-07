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

cd $ottoDir/$today

# 28 cluster jobs, 16 threads each --> 1/2 capacity of ku cluster.  (for many hours)
jobCount=28
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
/cluster/home/angie/github/usher/build/usher -T $threadCount -A -e 5 -v \$vcf -i \$pbIn -o \$pbOut >& \$log
EOF
chmod a+x runUsher.sh

cp /dev/null jobList
for ((i=0;  $i < $jobCount;  i++)); do
    echo "./runUsher.sh new.$i.masked.vcf.gz prevRenamed.pb {check out exists preTrim.$i.pb} usher.addNew.$i.log" >> jobList
done

ssh ku "cd $ottoDir/$today && para -cpu=$threadCount -ram=16g -retries=5 -priority=10 make jobList"

# Make a combined log (updateCombinedTree.sh looks there for sequences with excessively high EPPs)
cat usher.addNew.*.log > usher.addNew.log

# Merge 28 protobufs into 1
time $matUtils merge -T 8 --input-mat-1 preTrim.0.pb --input-mat-2 preTrim.1.pb -o merged.0.1.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.2.pb --input-mat-2 preTrim.3.pb -o merged.2.3.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.4.pb --input-mat-2 preTrim.5.pb -o merged.4.5.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.6.pb --input-mat-2 preTrim.7.pb -o merged.6.7.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.8.pb --input-mat-2 preTrim.9.pb -o merged.8.9.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.10.pb --input-mat-2 preTrim.11.pb -o merged.10.11.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.12.pb --input-mat-2 preTrim.13.pb -o merged.12.13.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.14.pb --input-mat-2 preTrim.15.pb -o merged.14.15.pb &
wait

time $matUtils merge -T 8 --input-mat-1 preTrim.16.pb --input-mat-2 preTrim.17.pb -o merged.16.17.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.18.pb --input-mat-2 preTrim.19.pb -o merged.18.19.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.20.pb --input-mat-2 preTrim.21.pb -o merged.20.21.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.22.pb --input-mat-2 preTrim.23.pb -o merged.22.23.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.24.pb --input-mat-2 preTrim.25.pb -o merged.24.25.pb &
$matUtils merge -T 8 --input-mat-1 preTrim.26.pb --input-mat-2 preTrim.27.pb -o merged.26.27.pb &
#$matUtils merge -T 8 --input-mat-1 preTrim.28.pb --input-mat-2 preTrim.29.pb -o merged.28.29.pb &
#$matUtils merge -T 8 --input-mat-1 preTrim.30.pb --input-mat-2 preTrim.31.pb -o merged.30.31.pb &
wait

time $matUtils merge -T 8 --input-mat-1 merged.0.1.pb --input-mat-2 merged.2.3.pb -o merged.0.3.pb &
$matUtils merge -T 8 --input-mat-1 merged.4.5.pb --input-mat-2 merged.6.7.pb -o merged.4.7.pb &
$matUtils merge -T 8 --input-mat-1 merged.8.9.pb --input-mat-2 merged.10.11.pb -o merged.8.11.pb &
$matUtils merge -T 8 --input-mat-1 merged.12.13.pb --input-mat-2 merged.14.15.pb -o merged.12.15.pb &
$matUtils merge -T 8 --input-mat-1 merged.16.17.pb --input-mat-2 merged.18.19.pb -o merged.16.19.pb &
$matUtils merge -T 8 --input-mat-1 merged.20.21.pb --input-mat-2 merged.22.23.pb -o merged.20.23.pb &
$matUtils merge -T 8 --input-mat-1 merged.24.25.pb --input-mat-2 merged.26.27.pb -o merged.24.27.pb &
#$matUtils merge -T 8 --input-mat-1 merged.28.29.pb --input-mat-2 merged.30.31.pb -o merged.28.31.pb &
wait

time $matUtils merge -T 16 --input-mat-1 merged.0.3.pb --input-mat-2 merged.4.7.pb -o merged.0.7.pb &
$matUtils merge -T 16 --input-mat-1 merged.8.11.pb --input-mat-2 merged.12.15.pb -o merged.8.15.pb &
$matUtils merge -T 16 --input-mat-1 merged.16.19.pb --input-mat-2 merged.20.23.pb -o merged.16.23.pb &
#$matUtils merge -T 16 --input-mat-1 merged.24.27.pb --input-mat-2 merged.28.31.pb -o merged.24.31.pb &
wait

time $matUtils merge -T 32 --input-mat-1 merged.0.7.pb --input-mat-2 merged.8.15.pb -o merged.0.15.pb &
#time $matUtils merge -T 32 --input-mat-1 merged.16.23.pb --input-mat-2 merged.24.31.pb -o merged.16.31.pb &
time $matUtils merge -T 32 --input-mat-1 merged.16.23.pb --input-mat-2 merged.24.27.pb -o merged.16.27.pb &
wait

#time $matUtils merge -T 64 --input-mat-1 merged.0.15.pb --input-mat-2 merged.16.31.pb -o merged.pb
time $matUtils merge -T 64 --input-mat-1 merged.0.15.pb --input-mat-2 merged.16.27.pb -o merged.pb

# Mask Delta deletion sites where spurious "mutations" called by sloppy genome assembly pipelines
# wreak havoc otherwise.
$scriptDir/maskDelta.sh merged.pb merged.deltaMasked.pb

# Optimize on the cluster too, with Cheng's MPI-parallelized matOptimize
# Reserve parasol nodes with a "cluster run" of sleep commands
mkdir -p reserveKuNodes
cd reserveKuNodes
jobCount=16
threadCount=16
reserveHours=2
reserveSeconds=$((3600 * $reserveHours))
cp /dev/null jobList
for ((i=0;  $i < $jobCount;  i++)); do
    echo "sleep $reserveSeconds" >> jobList
done
ssh ku "cd $ottoDir/$today/reserveKuNodes && para create -cpu=$threadCount jobList && para push"

# Wait for all jobs to start (i.e. all nodes to be reserved):
echo "Waiting for parasol to assign ku nodes"
while (( $(ssh ku "parasol list jobs | grep $USER | grep sleep" | awk '$2 != "none"' | wc -l) < $jobCount )); do
    sleep 10
    # Just in case there was a crash (we seem to have some borderline nodes):
    ssh ku "cd $ottoDir/$today/reserveKuNodes && para push"
done

# Make hostfile for mpirun
cd $ottoDir/$today
ssh ku "parasol list jobs | grep $USER | grep sleep" \
| awk '{print $2;}' \
| sort \
| uniq -c \
| awk '{print $2, "slots=" '$threadCount'*$1;}' \
    > hostfile

# mpirun matOptimize on first host in hostfile
headNode=$(head -1 hostfile | awk '{print $1;}')
radius=8
ssh $headNode "cd $ottoDir/$today && \
               $scriptDir/kuRunMatOptimize.sh -T $threadCount -r $radius -M $reserveHours \
                   -S move_log.cluster -i merged.deltaMasked.pb \
                   -o gisaidAndPublic.$today.masked.preTrim.pb \
                   >& matOptimize.cluster.log"

# Release the ku nodes by stopping the parasol batch
ssh ku "cd $ottoDir/$today/reserveKuNodes && para stop || true"

# Clean up
rm -f preTrim.*.pb merged.*.*.pb
chmod 664 gis*.preTrim*.pb
