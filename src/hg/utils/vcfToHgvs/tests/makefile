kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

vcfToHgvs = ${DESTBINDIR}/vcfToHgvs

test:: loadTestSet PersonalisGroundTruth2017_agree PersonalisGroundTruth2017_disagree stopLoss
	${MAKE} clean

# Stable set of RefSeq transcript tables & files for testing Personalis variants:
loadTestSet:
	hgLoadSqlTab test -verbose=0 chromInfo ../../../lib/chromInfo.sql PGTdb/chromInfo.tab
	hgLoadSqlTab test -verbose=0 chromAlias ../../../lib/chromAlias.sql PGTdb/chromAlias.tab
	hgLoadGenePred test -genePredExt ncbiRefSeq PGTdb/ncbiRefSeq.gp.gz
	hgLoadPsl test -verbose=0 -table=ncbiRefSeqPsl PGTdb/ncbiRefSeqPsl.psl.gz
	zcat PGTdb/ncbiRefSeqCds.tab.gz \
	| hgLoadSqlTab test -verbose=0 ncbiRefSeqCds ~/kent/src/hg/lib/cdsSpec.sql stdin
	zcat PGTdb/ncbiRefSeqLink.tab.gz \
	| hgLoadSqlTab test -verbose=0 ncbiRefSeqLink ~/kent/src/hg/lib/ncbiRefSeqLink.sql stdin
	zcat PGTdb/ncbiRefSeqPepTable.tab.gz \
	| hgLoadSqlTab test -verbose=0 ncbiRefSeqPepTable ~/kent/src/hg/lib/pepPred.sql stdin
	gunzip -c PGTdb/seqNcbiRefSeq.rna.fa.gz > PGTdb/seqNcbiRefSeq.rna.fa
	hgLoadSeq test -verbose=0 -drop -seqTbl=seqNcbiRefSeq -extFileTbl=extNcbiRefSeq \
	  `pwd`/PGTdb/seqNcbiRefSeq.rna.fa


PersonalisGroundTruth2017_agree: mkdirs
	${vcfToHgvs} test input/$@.vcf output/$@.tab
	sed -re 's/:/\t/g' output/$@.tab \
	| awk -F"\t" '{ $$8 = gensub(/[.].*/, "", "", $$8); \
                        if (index($$4, $$8) == 1 && $$14 != $$5) {print;} }' \
	  > output/$@_differences.tab
	diff -u expected/$@.tab output/$@.tab
	diff -u /dev/null output/$@_differences.tab

PersonalisGroundTruth2017_disagree: mkdirs
	${vcfToHgvs} test input/$@.vcf output/$@.tab
	sed -re 's/:/\t/g' output/$@.tab \
	| awk -F"\t" '{ $$8 = gensub(/[.].*/, "", "", $$8); \
                        if (index($$4, $$8) == 1 && $$14 == $$5) {print;} }' \
	  > output/$@_matches.tab
	diff -u expected/$@.tab output/$@.tab
	diff -u /dev/null output/$@_matches.tab

PersonalisGroundTruth2017_patches: mkdirs
	${vcfToHgvs} hg38 input/$@.vcf output/$@.tab
	sed -re 's/:/\t/g' output/$@.tab \
	| awk -F"\t" '{ $$8 = gensub(/[.].*/, "", "", $$8); \
                        if (index($$4, $$8) == 1 && $$14 != $$5) {print;} }' \
	  > output/$@_differences.tab
	diff -u expected/$@.tab output/$@.tab
	diff -u /dev/null output/$@_differences.tab

stopLoss:
	${vcfToHgvs} hg38 input/$@.vcf stdout | sort -k1,1 -k2n,2n -k3,3 -k6,6 > output/$@.tab
	diff -u expected/$@.tab output/$@.tab

clean::
	rm -rf output
	rm -f PGTdb/seqNcbiRefSeq.rna.fa seqNcbiRefSeq.tab

mkdirs:
	@${MKDIR} output
