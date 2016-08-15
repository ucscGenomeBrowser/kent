kentSrc = ../../..
include ../../../inc/common.mk

# make VERBOSE=-verbose=2 will echo out positions as they convert

test:   bin bedPlus3 bed8 simpleTest minus enm001 chuckTest chuckBigTest enr223 scaffoldEndBug

# tests too slow to use in default test suite
testSlow: mm3 multiple

liftOver=${DESTBINDIR}/liftOver
liftOverMerge=${DESTBINDIR}/liftOverMerge

bed12:	mkdirs
	${liftOver} -hasBin -bedPlus=12 \
            input/bed12.bed \
            /cluster/data/hg16/bed/liftOver/hg16ToHg17.over.chain \
                output/bed12.good.bed output/bed12.bad.bed 2> /dev/null
	cat output/bed12.{good,bad}.bed | \
                diff - expected/bed12.bed 1>&2

bed8:	mkdirs
	${liftOver} \
            input/bed8.bed \
            /cluster/data/hg16/bed/liftOver/hg16ToHg17.over.chain.gz \
                output/bed8.good.bed output/bed8.bad.bed 2> /dev/null
	cat output/bed8.{good,bad}.bed | \
                diff - expected/bed8.bed 1>&2

bin:    mkdirs
	${liftOver} -hasBin -bedPlus=6\
            input/bin.bed \
            /cluster/data/mm2/bed/liftOver/mm2ToMm5.over.chain.gz \
                output/bin.good.bed output/bin.bad.bed 2> /dev/null
	cat output/bin.{good,bad}.bed | \
                diff - expected/bin.bed 1>&2

bedPlus3:	mkdirs
	${liftOver} -bedPlus=3 \
            input/bedPlus.bed \
            /cluster/data/mm2/bed/liftOver/mm2ToMm5.over.chain.gz \
                output/bedPlus3.good.bed output/bedPlus3.bad.bed 2> /dev/null
	cat output/bedPlus3.{good,bad}.bed | \
                diff - expected/bedPlus3.bed 1>&2

simpleTest:     mkdirs
	${liftOver} \
            input/mouseRegions.bed \
                /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr16.chain \
                    output/mouseRegions.good.bed \
                    output/mouseRegions.bad.bed 2> /dev/null
	cat output/mouseRegions.{good,bad}.bed | \
                diff - expected/mouseRegions.bed 1>&2

minus:	mkdirs
	${liftOver} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple ${VERBOSE} \
                input/enr223.S.bed \
                    /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr6.chain \
                    output/enr223.S.good.bed \
                    output/enr223.S.bad.bed 2> /dev/null
	cat output/enr223.S.{good,bad}.bed | \
                diff - expected/enr223.S.bed 1>&2

# test w/o multiple
enm001:	mkdirs
	${liftOver} -minMatch=.01 \
            input/enm001.bed \
            /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr7.chain \
                output/enm001.good.bed output/enm001.bad.bed 2> /dev/null
	cat output/enm001.{good,bad}.bed | \
                diff - expected/enm001.bed 1>&2

chuckTest:	mkdirs
	${liftOver} \
            input/chuckTest.bed \
            /cluster/data/mm2/bed/liftOver/mm2.mm5.liftOver/over/chr19.chain \
                output/chuckTest.good.bed output/chuckTest.bad.bed 2> /dev/null
	cat output/chuckTest.{good,bad}.bed | \
                diff - expected/chuckTest.bed 1>&2

chuckBigTest:	mkdirs
	${liftOver} \
            input/chuckBigTest.bed \
            /cluster/data/mm2/bed/liftOver/mm2ToMm5.over.chain.gz \
                output/chuckBigTest.good.bed output/chuckBigTest.bad.bed 2> /dev/null
	cat output/chuckBigTest.{good,bad}.bed | \
                diff - expected/chuckBigTest.bed 1>&2

# missing expected file
m7rat:	mkdirs
	${liftOver} ${VERBOSE} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple \
            input/m7rat.bed \
            /cluster/data/hg16/bed/blastz.rn3/axtChain/subset/chr19.chain \
                output/m7rat.good.bed output/m7rat.bad.bed 2> /dev/null
	${liftOverMerge} ${VERBOSE} -mergeGap=2000 \
                output/m7rat.good.bed output/m7rat.merged.bed
	cat output/m7rat.merged.bed | \
                diff - expected/m7rat.bed 1>&2

# problem with config setting needed to access chain table
m4mouse:	mkdirs
	${liftOver} ${VERBOSE} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple -chainTable=hg16.chainMm5\
            input/m4mouse.bed \
            /cluster/data/hg16/bed/blastz.mm5/axtChain/subset/chr22.chain \
                output/m4mouse.good.bed output/m4mouse.bad.bed 2> /dev/null
	cat output/m4mouse.{good,bad}.bed | \
                diff - expected/m4mouse.bed 1>&2

# differs from expected -- it doesn't merge (this function done by
# liftOverMerge now ?
mergeTest: enm006
enm006:	mkdirs
	${liftOver} ${VERBOSE} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple \
            input/enm006.bed \
            /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chrX.chain \
                output/enm006.good.bed output/enm006.bad.bed 2> /dev/null
	cat output/enm006.{good,bad}.bed | \
                diff - expected/enm006.bed 1>&2

# missing expected file
enr112:	mkdirs
	${liftOver} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple ${VERBOSE} \
                input/enr112.bed \
                    /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr2.chain \
                    output/enr112.good.bed \
                    output/enr112.bad.bed 2> /dev/null
	cat output/enr112.{good,bad}.bed | \
                diff - expected/enr112.bed 1>&2

# bug where genePred going at the exact end of a scaffold was not mapped
scaffoldEndBug:
	${liftOver} -genePred ${VERBOSE} input/scaffoldEndBug.gp input/scaffoldEndBug.chain \
		output/$@.good.gp output/$@.bad.gp 2> /dev/null
	cat output/$@.{good,bad}.gp | diff - expected/$@.gp 1>&2


orderTest: enr223
enr223:	mkdirs
	${liftOver} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple ${VERBOSE} \
            input/enr223.bed \
                /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr6.chain \
                    output/enr223.good.bed \
                    output/enr223.bad.bed 2> /dev/null
	cat output/enr223.{good,bad}.bed | \
                diff - expected/enr223.bed 1>&2

# too slow -- don't use in default test suite
mm3:	mkdirs
	${liftOver} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple ${VERBOSE}\
                /cluster/data/hg16/bed/encodeRegions/encodeRegions.bed \
                    /cluster/data/hg16/bed/liftOver/hg16Tomm3.chain \
                    output/mm3.all.good.bed \
                    output/mm3.all.bad.bed
	cat output/mm3.all.{good,bad}.bed | \
                diff - expected/mm3.all.bed 1>&2

# too slow -- don't use in default test suite
multiple:	mkdirs
	${liftOver} -minMatch=.01 minSizeT=4000 minSizeQ=20000 -multiple ${VERBOSE}\
                input/mm3.multiple.bed \
                    /cluster/data/hg16/bed/liftOver/hg16Tomm3.chain \
                    output/mm3.multiple.good.bed \
                    output/mm3.multiple.bad.bed
	cat output/mm3.multiple.{good,bad}.bed | \
                diff - expected/mm3.multiple.bed 1>&2

mkdirs:
	mkdir -p output

clean:
	rm -fr output
