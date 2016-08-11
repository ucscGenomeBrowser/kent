kentSrc = ../../../..

include ../../../../inc/common.mk

altPaths=${DESTBINDIR}/altPaths
genePredToPsl=${DESTBINDIR}/genePredToPsl
blat=${DESTBINDIR}/blat
altSplice=${DESTBINDIR}/altSplice
faToNib=${DESTBINDIR}/faToNib

IN_DIR=input
EXPECT_DIR=expected

# Order is important
TESTS = genePredToPslTest \
        apSimpleCassDistTest altSimpleCassTest \
        alt3Prime5PrimeCassTest apAlt35CassRetDistTest \
	apSoftEndsStartsDistTest
# Order is important to avoid rebuilding too
# much stuff on the fly.
allTests: in ${TESTS}
	@echo "Passed: All tests successful."

# Check to make sure that basic alt3, alt5, and cass splices
# have been created correctly, or at least consistently...
alt3Prime5PrimeCassTest: alt3Prime5PrimeCassAgx
	diff -u ${EXPECT_DIR}/alt3Prime5PrimeCass.gold.agx tmp.agx

# Check to make sure the simplest possible cass exon is created
altSimpleCassTest: simpleCassAgx
	diff -u ${EXPECT_DIR}/simpleCass.gold.agx tmp.agx

# Check for distance matrix creation in altPaths
apSimpleCassDistTest: simpleCassAgx
	${altPaths} tmp.agx tmp.splice -dumpDistMatrix=tmp.dist
	diff -u ${EXPECT_DIR}/apSimpleCassDist.gold.mat tmp.dist

apSoftEndsStartsDistTest: softEndsStartsAgx
	${altPaths} tmp.agx tmp.splice -dumpDistMatrix=tmp.dist
	diff -u ${EXPECT_DIR}/apSoftEndsStartsDist.gold.mat tmp.dist

apAlt35CassRetDistTest: alt3Prime5PrimeCassAgx
	${altPaths} tmp.agx tmp.splice -dumpDistMatrix=tmp.dist
	diff -u ${EXPECT_DIR}/apAlt35CassRetDist.gold.mat tmp.dist


genePredToPslTest:
	${genePredToPsl} -bedFormat ${IN_DIR}/testChrom.sizes ${IN_DIR}/testLoci.bed tmp.psl
	diff -u ${EXPECT_DIR}/testLoci.psl tmp.psl

# Create the input files necessary for tests.
in:
	${blat} ${IN_DIR}/chromTest.fa ${IN_DIR}/alt3Prime5PrimeCassAgx.fa \
               -fine -tileSize=6 ${IN_DIR}/alt3Prime5PrimeCassAgx.psl
	${blat} ${IN_DIR}/chromTest.fa ${IN_DIR}/simpleCass.fa \
              -fine -tileSize=6 ${IN_DIR}/simpleCass.psl
	${blat} ${IN_DIR}/chromTest.fa ${IN_DIR}/altEndsStarts.fa \
               -fine -tileSize=6 ${IN_DIR}/altEndsStarts.psl
	${blat} ${IN_DIR}/chromTest.fa ${IN_DIR}/mutExclusive.fa \
               -fine -tileSize=6 ${IN_DIR}/mutExclusive.psl
	${blat} ${IN_DIR}/chromTest.fa ${IN_DIR}/altRetInt.fa \
               -fine -tileSize=6 ${IN_DIR}/altRetInt.psl
	${faToNib} ${IN_DIR}/chromTest.fa ${IN_DIR}/chromTest.nib

# Clean up the temporary files created in tests.
clean:
	rm -f ${IN_DIR}/chromTest.nib ${IN_DIR}/alt3Prime5PrimeCassAgx.psl \
	${IN_DIR}/simpleCass.psl tmp.agx ${IN_DIR}/altEndsStarts.psl \
	${IN_DIR}/altRetInt.psl ${IN_DIR}/mutExclusive.psl \
	tmp.psl

# Make the basic agx for tests.
alt3Prime5PrimeCassAgx: 
	${altSplice} -consensus -db=hg16 -beds=${IN_DIR}/testLoci.bed \
          -agxOut=tmp.agx  -chromNib=${IN_DIR}/chromTest.nib \
          -skipTissues -localMem -pslFile=${IN_DIR}/alt3Prime5PrimeCassAgx.psl >& /dev/null

# Make the basic agx for tests.
simpleCassAgx: 
	${altSplice} -consensus -db=hg16 -beds=${IN_DIR}/testLoci.bed \
          -agxOut=tmp.agx  -chromNib=${IN_DIR}/chromTest.nib \
          -skipTissues -localMem -pslFile=${IN_DIR}/simpleCass.psl >& /dev/null

# agx with some soft ends.
softEndsStartsAgx:
	${altSplice} -consensus -db=hg16 -beds=${IN_DIR}/testLoci.bed \
          -agxOut=tmp.agx  -chromNib=${IN_DIR}/chromTest.nib \
          -skipTissues -localMem -pslFile=${IN_DIR}/altEndsStarts.psl >& /dev/null

# mutual exclusive exons
mutExclusiveAgx:
	${altSplice} -consensus -db=hg16 -beds=${IN_DIR}/testLoci.bed \
          -agxOut=tmp.agx  -chromNib=${IN_DIR}/chromTest.nib \
          -skipTissues -localMem -pslFile=${IN_DIR}/mutExclusive.psl >& /dev/null

# mutual exclusive exons
altRetIntAgx:
	${altSplice} -consensus -db=hg16 -beds=${IN_DIR}/testLoci.bed \
          -agxOut=tmp.agx  -chromNib=${IN_DIR}/chromTest.nib \
          -skipTissues -localMem -pslFile=${IN_DIR}/altRetInt.psl >& /dev/null
