#!/bin/bash

grep "^GC" ../species.list | while read acc
do
  gcX="${acc:0:3}"
  d0="${acc:4:3}"
  d1="${acc:7:3}"
  d2="${acc:10:3}"
  srcDir="/hive/data/genomes/asmHubs/${gcX}/${d0}/${d1}/${d2}/${acc}"
  if [ -d "${srcDir}" ]; then
    sizes="${srcDir}/${acc}.chrom.sizes.txt"
    if [ -s "${sizes}" ]; then
      ln -s "${sizes}" "${acc}.len"
    else
      printf "ERROR: can not find sizes:\n%s\n" "${sizes}"
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
    sizes="${srcDir}/chrom.sizes"
    if [ -s "${sizes}" ]; then
      ln -s "${sizes}" "${db}.len"
    else
      printf "ERROR: can not find sizes:\n%s\n" "${sizes}"
      exit 255
    fi
  else
    printf "ERROR: can not find source directory:\n%s\n" "${srcDir}"
    exit 255
  fi
done

ls *.len > sizes
head sizes

# GCF_963930625.1
# GCF_964237555.1
# hg38
# hs1
# mm39
