# make VERBOSE=-verbose=2 will echo out positions as they convert

test:   simpleTest 

PROG=liftOver

simpleTest:     mkdirs
	${HOME}/bin/${MACHTYPE}/${PROG} \
                input/mouseRegions.bed \
                    input/hg16ToMm3.chr16.chain \
                    output/mouseRegions.good.bed \
                    output/mouseRegions.bad.bed
	cat output/mouseRegions.{good,bad}.bed | \
                diff - expected/mouseRegions.bed

minus:	mkdirs
	${HOME}/bin/${MACHTYPE}/${PROG} -minMatch=.01 -multiple ${VERBOSE} \
                input/enr223.S.bed \
                    /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr6.chain \
                    output/enr223.S.good.bed \
                    output/enr223.S.bad.bed
	cat output/enr223.S.{good,bad}.bed | \
                diff - expected/enr223.S.bed

enm001:	mkdirs
	${HOME}/bin/${MACHTYPE}/${PROG} -minMatch=.01 -multiple \
            input/enm001.bed \
            /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr7.chain \
                output/enm001.good.bed output/enm001.bad.bed
	cat output/enm001.{good,bad}.bed | \
                diff - expected/enm001.bed

enm006:	mkdirs
	${HOME}/bin/${MACHTYPE}/${PROG} -minMatch=.01 -multiple \
            input/enm006.bed \
            /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chrX.chain \
                output/enm006.good.bed output/enm006.bad.bed
	cat output/enm006.{good,bad}.bed | \
                diff - expected/enm006.bed

enr223:	mkdirs
	${HOME}/bin/${MACHTYPE}/${PROG} -minMatch=.01 -multiple \
                input/enr223.bed \
                    /cluster/data/hg16/bed/blastz.mm3/axtChain/subset/chr6.chain \
                    output/enr223.good.bed \
                    output/enr223.bad.bed
	cat output/enr223.{good,bad}.bed | \
                diff - expected/enr223.bed

mm3:	mkdirs
	${HOME}/bin/${MACHTYPE}/${PROG} -minMatch=.01 -multiple ${VERBOSE}\
                /cluster/data/hg16/bed/encodeRegions/encodeRegions.bed \
                    /cluster/data/hg16/bed/liftOver/hg16Tomm3.chain \
                    output/mm3.all.good.bed \
                    output/mm3.all.bad.bed
	cat output/mm3.all.{good,bad}.bed | \
                diff - expected/mm3.all.bed

multiple:	mkdirs
	${HOME}/bin/${MACHTYPE}/${PROG} -minMatch=.01 -multiple ${VERBOSE}\
                input/mm3.multiple.bed \
                    /cluster/data/hg16/bed/liftOver/hg16Tomm3.chain \
                    output/mm3.multiple.good.bed \
                    output/mm3.multiple.bad.bed
	cat output/mm3.multiple.{good,bad}.bed | \
                diff - expected/mm3.multiple.bed

mkdirs:
	mkdir -p output

clean:
	rm -fr output
