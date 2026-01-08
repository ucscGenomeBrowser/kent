#!/bin/bash
set -o errexit -o pipefail
export PATH=$PATH:/cluster/bin/x86_64/ # for bedSort and bedToBigBed
cd /hive/data/outside/otto/clinvar/
if [ "$1" == "--alpha" ]; then
        ./clinVarToBed --auto --alpha
        #./clinvarSubLolly 0 --alpha
elif [ "$1" == "-nocheck" ]; then
        ./clinVarToBed --auto
else
        ./clinVarToBed --auto --maxDiff 0.1 --onlyNew 2>&1 | tee -a log/clinvar.log
fi
