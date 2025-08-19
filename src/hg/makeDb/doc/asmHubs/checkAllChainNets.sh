#!/bin/bash

cat ../fishAsmHub/hasChainNets.txt \
../legacyAsmHub/hasChainNets.txt \
../mammalsAsmHub/hasChainNets.txt \
../primatesAsmHub/hasChainNets.txt \
../vertebrateAsmHub/hasChainNets.txt \
../viralAsmHub/hasChainNets.txt > all.hasChainNets.txt

./updateLiftOverChain.pl all.hasChainNets.txt
