kentSrc = ../../..
include ../../../inc/common.mk

pslToChain=${DESTBINDIR}/pslToChain
chainToPsl=${DESTBINDIR}/chainToPsl

test: example1 example2 example3
	${MAKE} clean

example1: mkdirs
example1: mkdirs
	${pslToChain} input/ucscGenes.psl.gz output/ucscGenes.chain
	zcat input/ucscGenes.psl.gz | cut -f 10- > output/ucscGenes.shortPsl 
	${chainToPsl} output/ucscGenes.chain /cluster/data/hg38/chrom.sizes \
		input/ucscGenes.sizes.gz  /cluster/data/hg38/hg38.2bit  \
		input/ucscGenes.fa.gz stdout 2> /dev/null \
		| cut -f 10- > output/roundTrip.shortPsl  
	diff output/ucscGenes.shortPsl output/roundTrip.shortPsl

example2: mkdirs
	-${pslToChain} input/mtor.psl output/mtor.chain 2> output/error || true
	diff expected/error output/error

example3: mkdirs
	-${pslToChain} -fixStrand input/mtor.psl output/$@.chain
	diff expected/$@.chain output/$@.chain

mkdirs:
	@${MKDIR} output


clean::
	@rm -rf output
