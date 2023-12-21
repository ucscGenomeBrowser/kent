ifeq (${IDXSIZE},64)
blat = blat64
else
blat = blat
endif

test::
	${MAKE} test_idxsize
	${MAKE} test_idxsize IDXSIZE=64
	@echo "Note: to run tests on a 40 gbase genome run: make hugeTests" 

test_idxsize: tBasic tThrowback tIntronMax

tBasic:
	@mkdir -p output/basic
	${blat} -verbose=0 basic/hCrea.geno basic/hCrea.mrna output/basic/testRna.psl
	diff basic/refRna.psl output/basic/testRna.psl
	${blat} -verbose=0 -prot basic/hCrea.pep basic/mCrea.pep output/basic/testProt.psl
	diff basic/refProt.psl output/basic/testProt.psl
	${blat} -verbose=0 -t=dnax -q=prot basic/hCrea.geno basic/mCrea.pep output/basic/testProtX.psl
	diff basic/refProtX.psl output/basic/testProtX.psl
	${blat} -verbose=0 -t=dnax -q=rnax basic/hCrea.geno basic/mCrea.mrna output/basic/testRnaX.psl
	diff basic/refRnaX.psl output/basic/testRnaX.psl
	${blat} -verbose=0 -fine basic/hCrea.geno basic/hCrea.mrna output/basic/testFine.psl
	diff basic/refFine.psl output/basic/testFine.psl


tThrowback:
	@mkdir -p output/v29skips
	${blat} -verbose=0 throwback/target1.fa throwback/query1.fa output/throwback/test.psl
	pslCheck -verbose=0 output/throwback/test.psl
	diff throwback/test.psl output/throwback/test.psl
	${blat} -verbose=0 v29skips/ex1_database.fa v29skips/ex1_query.fa output/v29skips/ex1.psl
	diff v29skips/ex1.psl output/v29skips/ex1.psl
	${blat} -verbose=0 v29skips/ex2_database.fa v29skips/ex2_query.fa output/v29skips/ex2.psl
	diff v29skips/ex2.psl output/v29skips/ex2.psl

tIntronMax:
	mkdir -p output/intron50k
	${blat} -verbose=0 intron50k/target.fa intron50k/query.fa output/intron50k/test1.psl -minScore=190
	diff intron50k/test1.psl output/intron50k/test1.psl
	${blat} -verbose=0 intron50k/target.fa intron50k/query.fa output/intron50k/test2.psl -minScore=190 -maxIntron=40000
	diff intron50k/test2.psl output/intron50k/test2.psl
	${blat} -verbose=0 intron50k/target.fa intron50k/query.fa output/intron50k/test3.psl -minScore=190 -maxIntron=5000
	diff intron50k/test3.psl output/intron50k/test3.psl

huge2bit = /hive/data/genomes/asmHubs/GCF/019/279/795/GCF_019279795.1/GCF_019279795.1.2bit
hugeInputDir = ../../gfServer/tests/input

hugeTests: tHugeUntrans tHugeTrans tHugeProt

tHugeUntrans:
	@mkdir -p output/hugeUntrans
	blat64 -verbose=0 ${huge2bit} ${hugeInputDir}/lungfish.rna.fa output/hugeUntrans/lungfish.rna.psl
	pslCheck -verbose=0 output/hugeUntrans/lungfish.rna.psl
	diff hugeUntrans/lungfish.rna.psl output/hugeUntrans/lungfish.rna.psl

tHugeTrans:
	@mkdir -p output/hugeTrans
	blat64 -verbose=0 -t=dnax -q=rnax ${huge2bit} ${hugeInputDir}/lungfish.rna.fa output/hugeTrans/lungfish.rna.psl
	pslCheck -verbose=0 output/hugeTrans/lungfish.rna.psl
	diff hugeTrans/lungfish.rna.psl output/hugeTrans/lungfish.rna.psl

tHugeProt:
	@mkdir -p output/hugeProt
	blat64 -verbose=0 -t=dnax -q=prot ${huge2bit} ${hugeInputDir}/lungfish.prot.fa output/hugeProt/lungfish.prot.psl
	diff hugeProt/lungfish.prot.psl output/hugeProt/lungfish.prot.psl

clean::
	rm -rf output
