#!/bin/csh
cp ./$1 /scratch/
cp ./$2 /scratch/
semiNorm /scratch/$1:t /scratch/$2:t $3 $4 > /scratch/$2:t:r.s.txt
cp /scratch/$2:t:r.s.txt ./
rm /scratch/$1:t
rm /scratch/$2:t
rm /scratch/$2:t:r.s.txt
