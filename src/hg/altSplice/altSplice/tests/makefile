
IN_DIR=input
EXPECT_DIR=expected


allTests: in alt3Prime5PrimeCassTest altSimpleCassetteTest

# Check to make sure that basic alt3, alt5, and cassette splices
# have been created correctly, or at least consistently...
alt3Prime5PrimeCassTest: alt3Prime5PrimeCassAgx
	diff -u $(EXPECT_DIR)/alt3Prime5PrimeCass.gold.agx test.agx

# Check to make sure the simplest possible cassette exon is created
altSimpleCassetteTest: simpleCassetteAgx
	diff -u $(EXPECT_DIR)/simpleCassette.gold.agx test.agx

# Create the input files necessary for tests.
in:
	blat $(IN_DIR)/chromTest.fa $(IN_DIR)/alt3Prime5PrimeCassAgx.fa \
               -fine -tileSize=6 $(IN_DIR)/alt3Prime5PrimeCassAgx.psl
	blat $(IN_DIR)/chromTest.fa $(IN_DIR)/simpleCassette.fa \
              -fine -tileSize=6 $(IN_DIR)/simpleCassette.psl
	faToNib $(IN_DIR)/chromTest.fa $(IN_DIR)/chromTest.nib

# Clean up the temporary files created in tests.
clean:
	rm -f $(IN_DIR)/chromTest.nib $(IN_DIR)/alt3Prime5PrimeCassAgx.psl \
	 $(IN_DIR)/simpleCassette.psl test.agx 

# Make the basic agx for tests.
alt3Prime5PrimeCassAgx: 
	altSplice -consensus -db=hg16 -beds=$(IN_DIR)/testLoci.bed \
          -agxOut=test.agx  -chromNib=$(IN_DIR)/chromTest.nib \
          -skipTissues -localMem -pslFile=$(IN_DIR)/alt3Prime5PrimeCassAgx.psl >& /dev/null

# Make the basic agx for tests.
simpleCassetteAgx: 
	altSplice -consensus -db=hg16 -beds=$(IN_DIR)/testLoci.bed \
          -agxOut=test.agx  -chromNib=$(IN_DIR)/chromTest.nib \
          -skipTissues -localMem -pslFile=$(IN_DIR)/simpleCassette.psl >& /dev/null
