all:  tThrowback tIntronMax

tThrowback:
	blat -verbose=0 throwback/target1.fa throwback/query1.fa throwback/test.psl
	pslCheck -verbose=0 throwback/test.psl
	blat -verbose=0 v29skips/ex1_database.fa v29skips/ex1_query.fa v29skips/ex1.psl
	diff v29skips/ex1_reference.psl v29skips/ex1.psl
	blat -verbose=0 v29skips/ex2_database.fa v29skips/ex2_query.fa v29skips/ex2.psl
	diff v29skips/ex2_reference.psl v29skips/ex2.psl

tIntronMax:
	mkdir -p intron50k/out
	blat -verbose=0 intron50k/target.fa intron50k/query.fa intron50k/out/test1.psl -minScore=190
	diff intron50k/expected/test1.psl intron50k/out/test1.psl
	blat -verbose=0 intron50k/target.fa intron50k/query.fa intron50k/out/test2.psl -minScore=190 -maxIntron=40000
	diff intron50k/expected/test2.psl intron50k/out/test2.psl
	blat -verbose=0 intron50k/target.fa intron50k/query.fa intron50k/out/test3.psl -minScore=190 -maxIntron=5000
	diff intron50k/expected/test3.psl intron50k/out/test3.psl
	rm -rf intron50k/out

clean::
	@echo nothing to clean in blat/test/
