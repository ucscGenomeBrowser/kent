#!/bin/bash
set -beEu -o pipefail
set -x

# One-time thing: split RefSeq Assembly _genomic.gbff.gz files into one file per NC_ accession.

fluAScriptDir=$(dirname "${BASH_SOURCE[0]}")

fluADir=/hive/data/outside/otto/fluA

assemblyDir=/hive/data/outside/ncbi/genomes

for asmAcc in GCF_000865085.1 GCF_001343785.1 GCF_000865725.1 GCF_000928555.1 GCF_000864105.1 \
              GCF_000866645.1 GCF_000851145.1; do
    asmDir=$(echo $asmAcc \
        | sed -re 's@^(GC[AF])_([0-9]{3})([0-9]{3})([0-9]{3})\.([0-9]+)@\1/\2/\3/\4/\1_\2\3\4.\5@')
    assemblyGbff=$assemblyDir/$asmDir*/$asmAcc*_genomic.gbff.gz
    assemblyReport=$assemblyDir/$asmDir*/$asmAcc*_assembly_report.txt
    segRefs=$(tawk '$8 == "Primary Assembly" {print $7;}' $assemblyReport)

    mkdir -p $fluADir/$asmAcc
    pushd $fluADir/$asmAcc
    gunzip -c $assemblyGbff | $fluAScriptDir/splitGbff.pl
    for segRef in $segRefs; do
        segRefNoDot=$(echo $segRef | sed -re 's/\.[0-9]$//;')
        if [ -s $segRefNoDot.gbff ]; then
            segRefInFile=$(grep ^VERSION $segRefNoDot.gbff | awk '{print $2;}')
            mv $segRefNoDot.gbff $segRefInFile.gbff
        else
            echo "*** ERROR: expected to find $segRefNoDot.gbff from $assemblyGbff but it's not there ***"
        fi
    done
    popd
done
