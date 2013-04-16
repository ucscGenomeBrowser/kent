PROG=netToAxt

test: clean all

all:    split noSplit

split:  mkdirs
	${PROG} input/testDog.net input/testDog.chain /cluster/data/hg17/nib \
                /cluster/data/canFam1/nib output/testDog.split.axt
	diff expected/testDog.split.axt output

noSplit: mkdirs
	${PROG} input/testDog.net input/testDog.chain /cluster/data/hg17/nib \
                -noSplit /cluster/data/canFam1/nib output/testDog.noSplit.axt
	diff expected/testDog.noSplit.axt output

mkdirs:
	mkdir -p output
clean:
	rm -fr output
