#!/bin/csh -ef
cp $1 /scratch/$1.$$
cp $2 /scratch/
/cluster/home/weber/bin/cdf /scratch/$1.$$ /scratch/$2 > /scratch/$2:r:r:r.L.txt
cp /scratch/$2:r:r:r.L.txt ./
rm /scratch/$1.$$
rm /scratch/$2 /scratch/$2:r:r:r.L.txt
