kentSrc = ../../..
A = ${DESTBINDIR}/pafToPsl
include ../../../inc/common.mk

test: hg38ChrMt indelEdges

hg38ChrMt: outputDir
	${A} input/$@.paf output/$@.psl
	@diff expected/$@.psl output/$@.psl

# Synthetic records whose CIGAR begins and/or ends with an I or D operator
# (leading/trailing indel), on both + and - strand.  Real minimap2/FASTGA
# output routinely has this shape; cigarToPsl() has a dedicated code path
# to trim these off the ends of the alignment, and it's easy to get the
# block-shifting arithmetic wrong there without it showing up in a
# same-species whole-genome test like hg38ChrMt.
indelEdges: outputDir
	${A} input/$@.paf output/$@.psl
	@diff expected/$@.psl output/$@.psl

outputDir:
	@${MKDIR} -p output

clean:
	rm -fr output
