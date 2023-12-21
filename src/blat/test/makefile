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
	mkdir -p basic/out
	${blat} -verbose=0 basic/hCrea.geno basic/hCrea.mrna basic/out/testRna.psl
	diff basic/refRna.psl basic/out/testRna.psl
	${blat} -verbose=0 -prot basic/hCrea.pep basic/mCrea.pep basic/out/testProt.psl
	diff basic/refProt.psl basic/out/testProt.psl
	${blat} -verbose=0 -t=dnax -q=prot basic/hCrea.geno basic/mCrea.pep basic/out/testProtX.psl
	diff basic/refProtX.psl basic/out/testProtX.psl
	${blat} -verbose=0 -t=dnax -q=rnax basic/hCrea.geno basic/mCrea.mrna basic/out/testRnaX.psl
	diff basic/refRnaX.psl basic/out/testRnaX.psl
	${blat} -verbose=0 -fine basic/hCrea.geno basic/hCrea.mrna basic/out/testFine.psl
	diff basic/refFine.psl basic/out/testFine.psl
	rm -rf basic/out

tThrowback:
	${blat} -verbose=0 throwback/target1.fa throwback/query1.fa throwback/test.psl
	pslCheck -verbose=0 throwback/test.psl
	${blat} -verbose=0 v29skips/ex1_database.fa v29skips/ex1_query.fa v29skips/ex1.psl
	diff v29skips/ex1_reference.psl v29skips/ex1.psl
	${blat} -verbose=0 v29skips/ex2_database.fa v29skips/ex2_query.fa v29skips/ex2.psl
	diff v29skips/ex2_reference.psl v29skips/ex2.psl

tIntronMax:
	mkdir -p intron50k/out
	${blat} -verbose=0 intron50k/target.fa intron50k/query.fa intron50k/out/test1.psl -minScore=190
	diff intron50k/expected/test1.psl intron50k/out/test1.psl
	${blat} -verbose=0 intron50k/target.fa intron50k/query.fa intron50k/out/test2.psl -minScore=190 -maxIntron=40000
	diff intron50k/expected/test2.psl intron50k/out/test2.psl
	${blat} -verbose=0 intron50k/target.fa intron50k/query.fa intron50k/out/test3.psl -minScore=190 -maxIntron=5000
	diff intron50k/expected/test3.psl intron50k/out/test3.psl
	rm -rf intron50k/out

huge2bit = /hive/data/genomes/asmHubs/GCF/019/279/795/GCF_019279795.1/GCF_019279795.1.2bit
hugeInputDir = ../../gfServer/tests/input

hugeTests: tHugeUntrans tHugeTrans tHugeProt

tHugeUntrans:
	@mkdir -p hugeUntrans/out
	blat64 -verbose=0 ${huge2bit} ${hugeInputDir}/lungfish.rna.fa hugeUntrans/out/lungfish.rna.psl
	pslCheck -verbose=0 hugeUntrans/out/lungfish.rna.psl
	diff hugeUntrans/lungfish.rna.psl hugeUntrans/out/lungfish.rna.psl

tHugeTrans:
	@mkdir -p hugeTrans/out
	blat64 -verbose=0 -t=dnax -q=rnax ${huge2bit} ${hugeInputDir}/lungfish.rna.fa hugeTrans/out/lungfish.rna.psl
	pslCheck -verbose=0 hugeTrans/out/lungfish.rna.psl
	diff hugeTrans/lungfish.rna.psl hugeTrans/out/lungfish.rna.psl

tHugeProt:
	@mkdir -p hugeProt/out
	blat64 -verbose=0 -t=dnax -q=prot ${huge2bit} ${hugeInputDir}/lungfish.prot.fa hugeProt/out/lungfish.prot.psl
	diff hugeProt/lungfish.prot.psl hugeProt/out/lungfish.prot.psl

clean::
	rm -rf */out/
