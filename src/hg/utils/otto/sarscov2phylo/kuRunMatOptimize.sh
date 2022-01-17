#!/bin/bash

. ~/.bashrc
conda activate usher

set -beEu -x -o pipefail

mpirun \
    --mca btl vader,tcp,self \
    --hostfile hostfile \
    --prefix $CONDA_PREFIX \
    --pernode \
    /cluster/home/angie/github/usher/build.ku/matOptimize $*
