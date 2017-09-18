#!/bin/sh

export mitoAcc=NC_008066

wget -O ${mitoAcc}.fa \
 "https://www.ncbi.nlm.nih.gov/sviewer/viewer.fcgi?db=nuccore&dopt=fasta&sendto=on&id=$mitoAcc"

echo ">chrM" > chrM.fa
grep -v "^>" ${mitoAcc}.fa >> chrM.fa

export mSize=`faCount chrM.fa | grep total | awk '{print $2}'`

/bin/echo -e "chrM\t1\t$mSize\t1\tF\t$mitoAcc\t1\t$mSize\t+" > chrM.agp
