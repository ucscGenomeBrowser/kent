include ../../../inc/common.mk

# makefile with genePred and genePredReader tests

BIN_DIR = bin/$(MACHTYPE)
GENE_PRED_TESTER = ${BIN_DIR}/genePredTester
EXP_DIR = expected/genePred
OUT_DIR = output/genePred
DB=hg16
TEST_TBL=gpTest_${USER}


# Data file used by tests:
#   - refSeq.psl - selected RefSeq  PSLs used to generate genePred, some chosen
#     because of frame problems, etc.
#   - refSeq.cds - CDS for above.
#   - refSeqMin.gp - miminmal genePred, only required columns
#   - refSeqId.gp - with id
#   - refSeqName2.gp - with name2
#   - refSeqIdName2.gp - with id, name2
#   - refSeqFrame.gp - with cdsStat and frame fields (from mrnaToGene)


test: fileTests tableTests fromPslTests compatTblTests
	hgsql -e "drop table ${TEST_TBL}" ${DB}

###
# test of reading/writing tab-separated files.
###
fileTests: fileMinTest fileIdTest fileName2Test fileIdName2Test fileFrameTest fileFrameStatTest
doFileTest = ${MAKE} -f genePredTests.mk doFileTest

fileMinTest:
	${doFileTest} id=$@ inGp=refSeqMin.gp

fileIdTest:
	${doFileTest} id=$@ inGp=refSeqId.gp opts="-idFld"

fileName2Test:
	${doFileTest} id=$@ inGp=refSeqName2.gp opts="-name2Fld"

fileIdName2Test:
	${doFileTest} id=$@ inGp=refSeqIdName2.gp opts="-idFld -name2Fld"

fileFrameTest:
	${doFileTest} id=$@ inGp=refSeqFrame.gp opts="-cdsStatFld -exonFramesFld"

# this regress a bug parsing unk cds status
fileFrameStatTest:
	${doFileTest} id=$@ inGp=refSeqFrameStat.gp opts="-cdsStatFld -exonFramesFld"

# Recurisve target to run a file read/write test.  Will diff the output
# with the input, which should be identical.
# Expects the following variables to be set:
#  id - test id
#  inGp - genePred (omitting dir)
#  opts - genePredTester options to add
doFileTest: mkout
	${GENE_PRED_TESTER} ${opts} -needRows=5 -output=${OUT_DIR}/${id}.gp -info=${OUT_DIR}/${id}.info readFile input/genePred/${inGp}
	diff -u input/genePred/${inGp} ${OUT_DIR}/${id}.gp
	diff -u expected/genePred/${id}.info ${OUT_DIR}/${id}.info

###
# test of loading and reading database tables
###
tableTests: tableMinTest tableIdTest tableName2Test tableIdName2Test tableFrameTest tableFrameStatTest
doTableTest = ${MAKE} -f genePredTests.mk doTableTest

tableMinTest:
	${doTableTest} id=$@ inGp=refSeqMin.gp

tableIdTest:
	${doTableTest} id=$@ inGp=refSeqId.gp opts="-idFld"

tableName2Test:
	${doTableTest} id=$@ inGp=refSeqName2.gp opts="-name2Fld"

tableIdName2Test:
	${doTableTest} id=$@ inGp=refSeqIdName2.gp opts="-idFld -name2Fld"

tableFrameTest:
	${doTableTest} id=$@ inGp=refSeqFrame.gp opts="-cdsStatFld -exonFramesFld"

tableFrameStatTest:
	${doTableTest} id=$@ inGp=refSeqFrameStat.gp opts="-cdsStatFld -exonFramesFld"


# Recurisve target to run a table read/write test.  Will diff the output
# with the input, which should be identical.
# Expects the following variables to be set:
#  id - test id
#  inGp - genePred (omitting dir)
#  opts - genePredTester options to add
doTableTest: mkout
	${GENE_PRED_TESTER} ${opts} -needRows=5 -output=${OUT_DIR}/${id}.gp loadTable ${DB} ${TEST_TBL} input/genePred/${inGp}
	${GENE_PRED_TESTER} ${opts} -needRows=5 -output=${OUT_DIR}/${id}.gp -info=${OUT_DIR}/${id}.info readTable ${DB} ${TEST_TBL}
	diff -u input/genePred/${inGp} ${OUT_DIR}/${id}.gp
	diff -u expected/genePred/${id}.info ${OUT_DIR}/${id}.info


###
# test of genePredFromPsl2 with various various optional fields
###
fromPslTests: fromPslMinTest fromPslCdsFrameTest
doFromPslTest = ${MAKE} -f genePredTests.mk doFromPslTest

fromPslMinTest:
	${doFromPslTest} id=$@ inBase=refSeq

fromPslCdsFrameTest:
	${doFromPslTest} id=$@ inBase=refSeq opts="-cdsStatFld -exonFramesFld"


# Recurisve target to run a file psl to genePred tests
#  id - test id
#  inBase - base name for psl and cds files
#  opts - genePredTester options to add
doFromPslTest: mkout
	${GENE_PRED_TESTER} ${opts} -output=${OUT_DIR}/${id}.gp fromPsl input/genePred/${inBase}.psl input/genePred/${inBase}.cds
	${GENE_PRED_TESTER} ${opts} readFile ${OUT_DIR}/${id}.gp
	diff -u ${EXP_DIR}/${id}.gp ${OUT_DIR}/${id}.gp


# tests of reading existing tables; these don't diff output, since tables
# are not stable, but at least see if we can read them
compatTblTests: refFlatTest refGeneTest knownGeneTest
doCompatTblTest = ${MAKE} -f genePredTests.mk doCompatTblTest

refFlatTest:
	${doCompatTblTest} id=$@ tbl=refFlat
refGeneTest:
	${doCompatTblTest} id=$@ tbl=refGene
knownGeneTest:
	${doCompatTblTest} id=$@ tbl=knownGene

# recursive target to read table
# id, tbl
doCompatTblTest: mkout
	${GENE_PRED_TESTER} -maxRows=1000 -needRows=1000 -info=${OUT_DIR}/${id}.info readTable ${DB} ${tbl}
	diff -u ${EXP_DIR}/${id}.info ${OUT_DIR}/${id}.info


mkout:
	@${MKDIR} ${OUT_DIR}
