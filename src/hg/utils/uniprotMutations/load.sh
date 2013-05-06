#!/bin/sh
cp uniprotMutations.hg19.bb /hive/data/genomes/hg19/bed/spMut/spMut.bb
ln -fs /hive/data/genomes/hg19/bed/spMut/spMut.bb /gbdb/hg19/bbi/spMut.bb
hgBbiDbLink hg19 spMut /hive/data/genomes/hg19/bed/spMut/spMut.bb

cp uniprotAnnotations.hg19.bb /hive/data/genomes/hg19/bed/spMut/spAnnot.bb
ln -fs /hive/data/genomes/hg19/bed/spMut/spAnnot.bb /gbdb/hg19/bbi/spAnnot.bb
hgBbiDbLink hg19 spAnnot /hive/data/genomes/hg19/bed/spMut/spAnnot.bb
