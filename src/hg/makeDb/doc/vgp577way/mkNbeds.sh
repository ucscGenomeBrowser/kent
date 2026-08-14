#!/bin/bash

mkdir -p nBedDir

grep "^GC" ../species.list | while read acc
do
  gcX="${acc:0:3}"
  d0="${acc:4:3}"
  d1="${acc:7:3}"
  d2="${acc:10:3}"
  srcDir="/hive/data/genomes/asmHubs/${gcX}/${d0}/${d1}/${d2}/${acc}"
  if [ -d "${srcDir}" ]; then
    twoBit="${srcDir}/${acc}.2bit"
    if [ -s "${twoBit}" ]; then
      twoBitInfo -nBed ${twoBit} nBedDir/${acc}.bed
      ln -s nBedDir/${acc}.bed ./
    else
      printf "ERROR: can not find twoBit:\n%s\n" "${twoBit}"
      exit 255
    fi
  else
    printf "ERROR: can not find source directory:\n%s\n" "${srcDir}"
    exit 255
  fi
done

# GCF_963930625.1
# GCF_964237555.1
# hg38
# hs1
# mm39

grep -v "^GC" ../species.list | while read db
do
  srcDir="/hive/data/genomes/${db}"
  if [ -d "${srcDir}" ]; then
    twoBit="${srcDir}/${db}.2bit"
    if [ -s "${twoBit}" ]; then
      twoBitInfo -nBed ${twoBit} nBedDir/${db}.bed
      ln -s nBedDir/${db}.bed ./
    else
      printf "ERROR: can not find 2bit:\n%s\n" "${twoBit}"
      exit 255
    fi
  else
    printf "ERROR: can not find source directory:\n%s\n" "${srcDir}"
    exit 255
  fi
done

ls *.bed > nBeds

wc -l nBeds sizes

# GCF_963930625.1
# GCF_964237555.1
# hg38
# hs1
# mm39
