include ../../../inc/common.mk

all:

test: basic srcPre dups impliedStop

# basic conversion
basic: mkdirs
	gtfToGenePred -infoOut=output/$@.info input/basic.gtf output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# source prefix filtering
srcPre: mkdirs
	gtfToGenePred  -infoOut=output/$@.info -sourcePrefix=mgc2 -sourcePrefix=mgc3 input/basic.gtf output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# error reporting
dups: mkdirs
	if gtfToGenePred -allErrors input/dups.gtf output/$@.gp >output/$@.out 2>&1 ; then false ; else true ; fi
	diff expected/$@.out output/$@.out
	diff expected/$@.gp output/$@.gp

impliedStop: mkdirs
	gtfToGenePred -impliedStopAfterCds -infoOut=output/$@.info input/flybase.gtf output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

mkdirs:
	mkdir -p output

clean:
	rm -rf output
