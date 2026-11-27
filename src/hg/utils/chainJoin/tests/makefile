kentSrc = ../../../..
include ../../../../inc/common.mk

chainJoin = ${DESTBINDIR}/chainJoin

all:: test

test:: joinTest noGapTest bigGapTest guideTest guideTrimTest sameEndTest maxGapTest

# Exact joins, a gap on the diagonal, indels (gaps that differ, one of them zero), overlaps
# inside the two blocks, the minus strand, a chain of three, and the cases that must not
# join: an overlap past the block, an overlap on one assembly only, another query sequence
# or strand, a gap over the limit on one or both assemblies.
joinTest: mkout
	${chainJoin} input/join.chain output/$@.chain 2> output/$@.err
	diff expected/$@.chain output/$@.chain
	diff expected/$@.err output/$@.err

# With -maxGap=0, only chains that touch or overlap on one diagonal are joined.
noGapTest: mkout
	${chainJoin} -maxGap=0 input/join.chain output/$@.chain 2> output/$@.err
	diff expected/$@.chain output/$@.chain
	diff expected/$@.err output/$@.err

# With a large -maxGap, chain 1 has two followers; the nearest (chain 2) must win,
# so chain 3 joins after it rather than stepping over it.  The gaps that were over the
# default limit (chains 18-19, and 23-24 with a 200 bp query gap) now join.
bigGapTest: mkout
	${chainJoin} -maxGap=500 input/join.chain output/$@.chain 2> output/$@.err
	diff expected/$@.chain output/$@.chain
	diff expected/$@.err output/$@.err

# hs1 chr17:83.92-84.00 Mb against H9 hap1 (GCA_054883195.1).  guideIn.chain is the liftOver
# chain file there, without the long chains; guide.chain is its blocks chained again with
# chainToPsl and axtChain.  The guide joins 34256, 42027, 13677, 36225, 10890 and 18813 into
# one chain.  36225 and 10890 overlap on H9 only (a tandem repeat), so 36225's tail and
# 10890's head are cut where the guide crosses, and kept as chains 1490288 and 1490289.
# The far first block of 18813 is cut off as 1490290.  Joins next to 251787 are refused,
# since the guide leaves that chain and comes back to it.
guideTest: mkout
	${chainJoin} -guide=input/guide.chain input/guideIn.chain output/$@.chain 2> output/$@.err
	diff expected/$@.chain output/$@.chain
	diff expected/$@.err output/$@.err

# With -maxTrim=20, the joins that cut 39 bp from 36225 and 135 bp from 18813 are refused.
guideTrimTest: mkout
	${chainJoin} -guide=input/guide.chain -maxTrim=20 input/guideIn.chain output/$@.chain 2> output/$@.err
	diff expected/$@.chain output/$@.chain
	diff expected/$@.err output/$@.err

# An exact duplicate, and a chain inside the last block of another that ends at the same
# base, must not be joined: neither reaches past the end of the chain before it.
sameEndTest: mkout
	${chainJoin} input/sameEnd.chain output/$@.chain 2> output/$@.err
	diff expected/$@.chain output/$@.chain
	diff expected/$@.err output/$@.err

# The largest -maxGap must not overflow and stop every join.
maxGapTest: mkout
	${chainJoin} -maxGap=2147483647 input/join.chain output/$@.chain 2> output/$@.err
	diff expected/$@.chain output/$@.chain
	diff expected/$@.err output/$@.err

mkout:
	@mkdir -p output

clean::
	rm -rf output
