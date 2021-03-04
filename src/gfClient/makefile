kentSrc = ..
A = gfClient
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
include $(kentSrc)/inc/userApp.mk
L += -lm $(SOCKETLIB)


hg38BlatServer = blat1d
hg38BlatPort = 17903
hg38GenomeDir = /hive/data/genomes/hg38
testCases = AB000095 AB000220 repeat test1 test2 test3

test:: ${testCases:%=%.test}

%.test:
	@mkdir -p output
	${BINDIR}/gfClient -nohead ${hg38BlatServer} ${hg38BlatPort} ${hg38GenomeDir} $*.fa output/$*.psl

clean::
	rm -rf output
