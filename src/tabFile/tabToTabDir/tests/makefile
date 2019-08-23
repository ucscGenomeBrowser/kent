kentSrc = ../../..
A = ${DESTBINDIR}/tabToTabDir
include ../../../inc/common.mk

test: test1

test1: outputDir
	# generate big wigs
	${A} input/in.tsv input/spec.txt output

	# Do the diff
	diff output expected

	# clean up temporary directories
	rm -r output tmp

clean::
	@rm -rf output
	@rm -rf tmp 

outputDir:
	@${MKDIR} -p output
	@${MKDIR} -p tmp
