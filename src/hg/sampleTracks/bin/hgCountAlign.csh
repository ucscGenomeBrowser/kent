#!/bin/csh -ef
cp $3 /scratch/
hgCountAlign $1 $2 /scratch/$3:t /scratch/$4:t
cp /scratch/$4:t ./
rm /scratch/$3:t /scratch/$4:t
