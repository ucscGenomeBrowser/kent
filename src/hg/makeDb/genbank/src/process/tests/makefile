kentSrc = ../../../../../..
GBROOT = ../../..
include ${GBROOT}/common.mk

VG=
#VG=valgrind --tool=memcheck --num-callers=100 --leak-check=full --leak-resolution=high --show-reachable=yes 

PROCESS = ${VG}${GB_BINARCH}/gbProcess

all:

test: refseqTest estTest mgcTest regressTest orfeomeTest ncrnaTest

refseqTest:
	${MAKE} doTest id=$@ input=refseq opts=-type=mrna

# AA225986 doesn't mention direction in description
estTest:
	${MAKE} doTest id=$@ input=ests opts=-type=est

# data like misc_difference needed for MGC
mgcTest:
	${MAKE} doTest id=$@ input=mgc opts=-type=mrna

# various regressions:
#   - D26070 - text in /note field gets parsed as CDS because a line starts with
#     `CDS'.
regressTest:
	${MAKE} doTest id=$@ input=regress opts=-type=mrna

# orfeome, some synthetic
orfeomeTest:
	${MAKE} doTest id=$@ input=orfeome opts=-type=mrna

# non-coding RNAs
ncrnaTest:
	${MAKE} doTest id=$@ input=ncrna opts=-type=mrna

# recursive target to run tests; args are: id, input
doTest:
	@${MKDIR} output
	${PROCESS} ${opts} output/${id}.fa output/${id}.ra input/${input}.gbff
	faSize -detailed output/${id}.fa > output/${id}.fasize
	diff expected/${id}.ra output/${id}.ra
	diff expected/${id}.fasize output/${id}.fasize

clean:
	rm -f output
