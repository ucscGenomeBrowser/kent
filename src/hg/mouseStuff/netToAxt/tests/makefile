kentSrc = ../../../..
include ../../../../inc/common.mk

netToAxt=${DESTBINDIR}/netToAxt

test: clean all

all:    split noSplit

split:  mkdirs
	${netToAxt} input/testDog.net input/testDog.chain /cluster/data/hg17/nib \
                /cluster/data/canFam1/nib output/testDog.split.axt > /dev/null 2>&1
	diff expected/testDog.split.axt output

noSplit: mkdirs
	${netToAxt} input/testDog.net input/testDog.chain /cluster/data/hg17/nib \
                -noSplit /cluster/data/canFam1/nib output/testDog.noSplit.axt > /dev/null 2>&1
	diff expected/testDog.noSplit.axt output

mkdirs:
	mkdir -p output
clean:
	rm -fr output
