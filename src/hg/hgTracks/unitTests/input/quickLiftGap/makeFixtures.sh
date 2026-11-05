#!/bin/bash
# makeFixtures.sh - make the files bigWigLoadPreDrawTester reads.  Run it here; the outputs are
# checked in, so it only needs running again to change them.  refs #38236
#
# gap.chain is one chain over chrRef:1000-9000 with two aligned blocks, 1000-2000 and
# 8000-9000, so a window inside 2000-8000 is inside the chain's span with no block in it.
# src.bw has a value in each block and one in the gap, which the lift must not show.
set -e
kentSrc=../../../../..
printf 'chrRef\t100000\n' > ref.chrom.sizes
printf 'chrSrc\t100000\n' > src.chrom.sizes
cat > gap.chain <<'CHAIN'
chain 2000 chrRef 100000 + 1000 9000 chrSrc 100000 + 1000 9000 1
1000	6000	6000
1000

CHAIN
chainToBigChain gap.chain gap.bigChain gap.bigLink
bedToBigBed -type=bed6+6 -as=$kentSrc/hg/lib/bigChain.as -tab gap.bigChain ref.chrom.sizes gap.bb
bedToBigBed -type=bed4+1 -as=$kentSrc/hg/lib/bigLink.as -tab gap.bigLink ref.chrom.sizes gap.link.bb
printf 'chrSrc\t1000\t2000\t5\nchrSrc\t4000\t5000\t3\nchrSrc\t8000\t9000\t7\n' > src.bedGraph
wigToBigWig src.bedGraph src.chrom.sizes src.bw
rm -f gap.bigChain gap.bigLink src.bedGraph
