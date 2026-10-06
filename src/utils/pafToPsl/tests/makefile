kentSrc = ../../..
A = ${DESTBINDIR}/pafToPsl
PSLCHECK = pslCheck
include ../../../inc/common.mk

test: hg38ChrMt indelEdges rm38480

hg38ChrMt: outputDir
	${A} input/$@.paf output/$@.psl
	@diff expected/$@.psl output/$@.psl
	@${PSLCHECK} output/$@.psl 2> output/$@.pslCheck
	@diff expected/$@.pslCheck output/$@.pslCheck

# Synthetic records whose CIGAR begins and/or ends with an I or D operator
# (leading/trailing indel), on both + and - strand.  Real minimap2/FASTGA
# output routinely has this shape; cigarToPsl() has a dedicated code path
# to trim these off the ends of the alignment, and it's easy to get the
# block-shifting arithmetic wrong there without it showing up in a
# same-species whole-genome test like hg38ChrMt.
indelEdges: outputDir
	${A} input/$@.paf output/$@.psl
	@diff expected/$@.psl output/$@.psl
	@${PSLCHECK} output/$@.psl 2> output/$@.pslCheck
	@diff expected/$@.pslCheck output/$@.pslCheck

# Regression cases for specific reported bugs:
#  qm - minus-strand record with an indel on only *one* end (unlike the
#       equal-leading/trailing-indel records in indelEdges, which can't
#       distinguish qStart being trimmed from qEnd being trimmed)
#  qn - an N (splice/intron gap) CIGAR operator, as from minimap2 -x splice
#  qa - adjacent indel operators (3I2D) with no intervening match, which
#       must not emit a zero-length block
#  qb, qc - PAF-reported coordinates that are out of range (qEnd > qSize)
#       or negative (qStart < 0): both must be rejected with a warning,
#       not silently wrapped through unsigned arithmetic, and must not
#       appear in the output
# pslCheck is run on the output here deliberately: a bad block-shift can
# still diff clean against a *wrong* expected file if the expected file was
# hand-derived incorrectly, so pslCheck's own independent invariants are
# the real backstop, not just this diff.
rm38480: outputDir
	${A} input/$@.paf output/$@.psl 2> output/$@.err
	@diff expected/$@.psl output/$@.psl
	@diff expected/$@.err output/$@.err
	@${PSLCHECK} output/$@.psl 2> output/$@.pslCheck
	@diff expected/$@.pslCheck output/$@.pslCheck

outputDir:
	@${MKDIR} -p output

clean:
	rm -fr output
