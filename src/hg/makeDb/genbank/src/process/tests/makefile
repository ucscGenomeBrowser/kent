GBROOT = ../../..
include ${GBROOT}/common.mk

VG=
#VG=valgrind --tool=memcheck --num-callers=100 --leak-check=full --leak-resolution=high --show-reachable=yes 

PROCESS = ${VG}${BINARCH}/gbProcess

all:

test: refseqTest estTest mgcTest

refseqTest:
	${MAKE} doTest id=$@ input=refseq

# AA225986 doesn't mention direction in description
estTest:
	${MAKE} doTest id=$@ input=ests

# data like misc_difference needed for MGC
mgcTest:
	${MAKE} doTest id=$@ input=mgc

# recursive target to run tests; args are: id, input
doTest:
	@${MKDIR} output
	${PROCESS} output/${id}.fa output/${id}.ra input/${input}.gbff
	faSize -detailed output/${id}.fa > output/${id}.fasize
	diff expected/${id}.ra output/${id}.ra
	diff expected/${id}.fasize output/${id}.fasize

clean:
	rm -f output
