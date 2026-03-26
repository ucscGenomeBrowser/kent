
# default test binary is the first hgGcPercent in the PATH
#  run with 'make user' to test your ~/bin/MACHTYPE/hgGcPercent
#  or 'make hgwdev' to test /cluster/bin/MACHTYPE/hgGcPercent

HGGCPERCENT ?= $(shell command -v hgGcPercent)

test::	test0 test1 test2

user::
	${MAKE} HGGCPERCENT=${HOME}/bin/${MACHTYPE}/hgGcPercent test

hgwdev::
	${MAKE} HGGCPERCENT=/cluster/bin/${MACHTYPE}/hgGcPercent test


# test0: wiggle output from local 2bit file
test0: setOutput
	${HGGCPERCENT} -wigOut -doGaps -win=5 -file=stdout -noLoad \
		-verbose=0 -noDots test inputs/test.2bit > testOutput/test0.out
	@diff expected/test0.out testOutput/test0.out

# test1: BED output from local 2bit file
test1: setOutput
	${HGGCPERCENT} -doGaps -win=5 -file=stdout -noLoad \
		-verbose=0 -noDots test inputs/test.2bit > testOutput/test1.out
	@diff expected/test1.out testOutput/test1.out

# test2: wiggle output from URL 2bit file (wuhCor1 SARS-CoV-2)
test2: setOutput
	${HGGCPERCENT} -wigOut -doGaps -win=5 -file=stdout -noLoad \
		-verbose=0 -noDots -chr=NC_045512v2 test \
		https://hgdownload.soe.ucsc.edu/goldenPath/wuhCor1/bigZips/wuhCor1.2bit \
		| md5sum | awk '{print $$1}' > testOutput/test2.md5sum
	@diff expected/url.md5sum testOutput/test2.md5sum

setOutput:
	@if [ ! -d testOutput ]; then mkdir testOutput; fi
	@echo "### testing binary: ${HGGCPERCENT}"

clean::
	rm -fr ./testOutput
