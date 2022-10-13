#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/maskDelta.sh

usage() {
    echo "usage: $0 treeIn.pb treeOut.pb"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

treeInPb=$1
treeOutPb=$2

ottoDir=/hive/data/outside/otto/sarscov2phylo

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

# I wish there were a less hacky method to identify the node for Delta, but since mutation
# paths can change as new samples are added, this is the most stable method I have at the moment:
# make sample-paths, grep for basal sample IND/GBRC714b/2021 (USA/WI-CDC-FG-038252/2021 would also
# work in case that one goes away for any reason), use the final node in path.
samplePaths=$treeInPb.sample-paths
$matUtils extract -i $treeInPb -S $samplePaths
deltaNode=$(grep IND/GBRC714b/2021 $samplePaths | awk '{print $NF;}' | sed -re 's/:.*//;')

# Delta has deletions at S:157-158 (22029-22034), ORF8:119-120 (28248-28253) and 28271.
# Mask those locations and some adjacent bases where we get a ton of spurious "mutations".
maskFile=$(basename $treeInPb .pb).branchSpecificMask.tsv
set +x
for ((i=22027;  $i <= 22034;  i++)); do
    echo -e "N${i}N\t$deltaNode"
done > $maskFile
for ((i=28246;  $i <= 28253;  i++)); do
    echo -e "N${i}N\t$deltaNode"
done >> $maskFile
echo -e "N28271N\t$deltaNode" >> $maskFile
set -x

# S:95 (21846) is also very unreliably detected in Delta.  Mask it off to avoid tree trouble,
# like split AY.100.
echo -e "N21846N\t$deltaNode" >> $maskFile

# These three sites are recommended for caution in the Problematic Sites set, and seem to have
# create a false lineage (AY.89) from samples that probably should be AY.4.  AY.89 is being
# withdrawn (https://github.com/cov-lineages/pango-designation/issues/398); mask sites in Delta.
echo -e "N21302N\t$deltaNode" >> $maskFile
echo -e "N21304N\t$deltaNode" >> $maskFile
echo -e "N21305N\t$deltaNode" >> $maskFile

# Mask flaky positions 28254 (ORF8:121) and 28461 (N:63) so that AY.96 is merged into AY.46
# https://github.com/cov-lineages/pango-designation/issues/435
echo -e "N28254N\t$deltaNode" >> $maskFile
echo -e "N28461N\t$deltaNode" >> $maskFile

# OK, not just Delta -- Alpha, Beta, Gamma and BA.2 have a deletion that causes spurious "mutations",
# especially at 11296 and 11291, somewhat also at 11288.
alphaNode=$(grep Italy/TAA-1900553896/2021 $samplePaths \
            | awk '{print $(NF-1);}' | sed -re 's/:.*//;')
betaNode=$(grep SouthAfrica/CERI-KRISP-K012031/2021 $samplePaths \
           | awk '{print $NF;}' | sed -re 's/:.*//;')
gammaNode=$(grep FRA/IHUCOVID-005193-N1/2021 $samplePaths | awk '{print $NF;}' | sed -re 's/:.*//;')
BA2Node=$(grep England/DHSC-CYBAB7G/2022 $samplePaths \
          | awk '{print $(NF-1);}' | sed -re 's/:.*//;')
set +x
for node in $alphaNode $betaNode $gammaNode $BA2Node; do
    for ((i=11288;  $i <= 11296;  i++)); do
        echo -e "N${i}N\t$node"
    done
done >> $maskFile
set -x

# BA.1 has almost the same deletion but it aligns 5 bases to the left, probably because it was
# combined with an SNV (https://github.com/cov-lineages/pango-designation/issues/361).
BA1Node=$(grep England/DHSC-CYBJ4Y8/2022 $samplePaths \
          | awk '{print $(NF-1);}' | sed -re 's/:.*//;')
set +x
for ((i=11283;  $i <= 11291;  i++)); do
    echo -e "N${i}N\t$BA1Node"
done >> $maskFile
# BA.1 has several other deletions that cause the same problem.
for ((i=6513;  $i <= 6515;  i++)); do
    echo -e "N${i}N\t$BA1Node"
done >> $maskFile
for ((i=21765;  $i <= 21770;  i++)); do
    echo -e "N${i}N\t$BA1Node"
done >> $maskFile
for ((i=21988;  $i <= 21995;  i++)); do
    echo -e "N${i}N\t$BA1Node"
done >> $maskFile
# There's a deletion at 22194-22196 and then an insertion after 22204.  The entire region
# between the deletion & insertion is riddled with noisy bases, and the noise continues
# well after the insertion point.  So mask from 22194 to 22217.
for ((i=22194;  $i <= 22217;  i++)); do
    echo -e "N${i}N\t$BA1Node"
done >> $maskFile
for ((i=28362;  $i <= 28370;  i++)); do
    echo -e "N${i}N\t$BA1Node"
done >> $maskFile

# BA.1 has a lot of amplicon dropout / Delta contam noise at these sites, but so far they
# don't affect the Delta/Omicron recombinants identified to date which all have Omicron spike,
# so mask these out in BA.1:
for i in 22813 22898 22882 23854; do
    echo -e "N${i}N\t$BA1Node"
done >> $maskFile

# BA.2 has some additional deletions.
for ((i=21633;  $i <= 21641;  i++)); do
    echo -e "N${i}N\t$BA2Node"
done >> $maskFile
# The deletion is supposed to be 28362-28370 but there is some uncertainty in placement and
# some edge effects to mask adjacent bases too.
for ((i=28361;  $i <= 28371;  i++)); do
    echo -e "N${i}N\t$BA2Node"
done >> $maskFile
for ((i=29734;  $i <= 29759;  i++)); do
    echo -e "N${i}N\t$BA2Node"
done >> $maskFile


# BA.2 also has some amplicon dropout problematic sites *** that hopefully won't mess up recombinants but I had better check ***
for i in 22786 22882 23854; do
    echo -e "N${i}N\t$BA2Node"
done >> $maskFile

# BA.2 has a lot of noise in these sites (some sites also noisy in Delta & BA.1 but no time for those)
# I would include 210 but that's important for Delta/BA.2 recombinants.  I would include 212 too,
# but will leave it in as a red flag that 210 may be bogus.
# 29766 seems to be found exclusively by Luxembourg and makes a mini-BA.2.
for i in 197 198 199 200 201 203 204 206 207 214 216 217 218 219 221 222 223 224 225 228 230 231 232 233 239 241 242 243 245 246 29766 ; do
    echo -e "N${i}N\t$BA2Node"
done >> $maskFile

# BA.4 and BA.5 have this deletion like BA.1:
BA4Node=$(grep SouthAfrica/NICD-N41664/2022 $samplePaths \
          | awk '{print $NF;}' | sed -re 's/:.*//;')
BA5Node=$(grep England/PHEP-YYFJPAM/2022 $samplePaths \
          | awk '{print $NF;}' | sed -re 's/:.*//;')
for ((i=21765;  $i <= 21770;  i++)); do
    echo -e "N${i}N\t$BA4Node"
    echo -e "N${i}N\t$BA5Node"
done >> $maskFile

# 28877 and 28878 together are highly homoplasic in all of B.1.1 (28881-28883).  They seem to be
# found very consistently in P.1*, but pop up in many places in Alpha and Omicrons.  I haven't
# looked closely at the Alpha instances but they definitely cause some mini-Omicrons
# (https://github.com/cov-lineages/pango-designation/issues/988).  Mask in BA.2 (which also covers
# BA.4 and BA.5); possibly could also mask in B.1.1.7 and BA.1 but those are old news.
for i in 28877 28878; do
    echo -e "N${i}N\t$BA2Node"
done >> $maskFile

# Also noticing a lot of noise on these 5'UTR locations (now I see why pangolin masks entire UTRs):
for i in 76 77 78 79 80 81 83 84 85 86 88 89 91 92 93 94 96 97 98 99 100 123 124 126 127 129 130 131 132 133 134 135 136 139 140 141 143 144 145 146 147 148 151 152 154 157 158 159 162 164 179 180; do
    echo -e "N${i}N\t$BA2Node"
done >> $maskFile

# BA.4-specific deletion:
for ((i=686;  $i <= 694;  i++)); do
    echo -e "N${i}N\t$BA4Node"
done >> $maskFile

# BA.2.75 has an awful lot of amplicon dropout problematic sites as of Sep. 2022.
BA275Node=$(grep India/WB-INSACOG-1931503209307/2022 $samplePaths \
          | awk '{print $NF;}' | sed -re 's/:.*//;')
# These sites I would mask in all of BA.2 if it weren't for wanting to find legit recombinants:
for backMut in G670T T2790C T3037C T4321C G9424A T9534C T9866C T10029C T10198C G18163A T19955C \
    G20055A T21618C G22200T A22578G T22674C C22679T T22686C G22688A A22775G T22813G A22992G A22995C \
    C23013A G23055A T23063A C23075T G23403A T23525C G23599T A23604C T23948G T24424A A24469T T25000C \
    T26270C G26577C T27807C T28271A C29510A ; do
    echo -e "$backMut\t$BA275Node"
done >> $maskFile
# These are BA.2.75-defining but prone to dropout.  Make sure we just wipe out reversions,
# not the BA.2.75 alleles that we want to keep on that node and subsequent nodes.
for backMut in T4586C G22001A C22016T A22033C G22190A C22577G G26275A; do
    echo -e "$backMut\t$BA275Node"
done >> $maskFile
# These BA.2.75-defining sites are less bad off, might want to back off on these if/when there
# are 10s of thousands of BA.2.75 sequences & more chances for recombinants:
for backMut in T3796C T3927C T5183C G12444A A15451G G22190A A22331G A22898G G22942T C23013A; do
    echo -e "$backMut\t$BA275Node"
done >> $maskFile

# BA.5 reversions causing big yuck branches
for backMut in G670T T2790C T3037C A4184G T4321C G9424A T9534C T10198C A12160G T15714C T17410C \
    G18163A T19955C G20055A T21618C G22200T A22578G T22674C C22679T T22686C G22688A A22775G T22813G \
    G22882T G22917T A22992G A22995C C23013A G23018T G23055A T23063A C23075T G23403A T23525C G23599T \
    A23604C T25000C A26529G G26577C A26709G T27807C T27889C T28271A C29510A ; do
    echo -e "$backMut\t$BA5Node"
done >> $maskFile
# The path to XAV should have G12160A and C27889T, but those cause too much trouble in BA.5
# so it will have to be a little incorrect.
# Likewise, XAN path should have G12160A but oh well.

set -x

time $matUtils mask -i $treeInPb \
    -m $maskFile \
    -o $treeOutPb

rm $samplePaths
