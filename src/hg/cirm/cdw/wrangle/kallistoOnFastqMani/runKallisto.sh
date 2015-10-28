#!/bin/bash
# need this bash wrapper, parasol cannot use the bash pipe syntax directly
# input are two SRA files with forward/reverse reads
# or alternatively two .fastq.gz files

# parameters:
# $1 = index
# $2 = output dir
# $3 = SRA input file
# $2 = SRA input file 2

set -e
set -o pipefail

# required until Erich pushes some rpms to ku
# export LD_LIBRARY_PATH=/cluster/home/max/usr/lib:/cluster/software/lib

#redirect the output of this script to a log file in current dir
logPath=`dirname $2`/`basename $3 .sra`.log
echo writing log to $logPath
exec >  >(tee -a $logPath)
exec 2> >(tee -a $logPath >&2)

# handle our temp directory
TMPDIR=/scratch/tmp/kallisto/$$ # $$ is current PID
#trap "echo deleting $TMPDIR; rm -rf $TMPDIR" EXIT # make sure the tmpdir gets deleted if we error abort
if [ ! -d "$DIRECTORY" ]; then
    echo making $TMPDIR
    mkdir -p $TMPDIR
fi

myDir=`dirname $0`

file1Ext=${3##*.}
echo $file1Ext
if [[ ( $file1Ext == "gz" ) || ( $file1Ext == "fastq" ) ]]; then
       fname1=$3
       fname2=$4
       fname3=
       fname4=
else
        SRADIR=/cluster/home/max/usr/bin
        echo unpacking the sra file
        $SRADIR/fastq-dump $3 --split-files -O $TMPDIR
        $SRADIR/fastq-dump $4 --split-files -O $TMPDIR
        # output files of the sra dumper
        fname1=$TMPDIR/`basename $3 .sra`_1.fastq
        fname2=$TMPDIR/`basename $3 .sra`_2.fastq
        fname3=$TMPDIR/`basename $4 .sra`_1.fastq
        fname4=$TMPDIR/`basename $4 .sra`_2.fastq
fi

if [ $# -lt 4 ]; then
    echo running kallisto single 
    $myDir/kallisto quant -i $1 -o $2 $fname1 $fname2 $fname3 $fname4 --single -l 75 -s 1 
    exit
fi
echo running kallisto
$myDir/kallisto quant -i $1 -o $2 $fname1 $fname2 $fname3 $fname4
#rm -f $fname1 $fname2 $fname3 $fname4

