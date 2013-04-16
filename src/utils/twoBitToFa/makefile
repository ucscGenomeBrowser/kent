kentSrc = ../..
A = twoBitToFa
include $(kentSrc)/inc/userApp.mk
L += -lm ${SOCKETLIB}

test::
	rm -fr tests/output
	${MKDIR} tests/output
	twoBitToFa tests/input/testN.2bit tests/output/testN.fa
	twoBitToFa tests/input/testMask.2bit tests/output/testMask.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=1 -end=11 tests/output/ml_1_11.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=2 -end=10 tests/output/ml_2_10.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=3 -end=9 tests/output/ml_3_9.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=4 -end=8 tests/output/ml_4_8.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=5 -end=7 tests/output/ml_5_7.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=5 -end=6 tests/output/ml_5_6.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=6 -end=7 tests/output/ml_6_7.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=7 -end=8 tests/output/ml_7_8.fa
	twoBitToFa tests/input/testMask.2bit -seq=manyLower -start=8 -end=9 tests/output/ml_8_9.fa
	twoBitToFa tests/input/testMask.2bit:manyLower tests/output/spec_ml.fa
	twoBitToFa tests/input/testMask.2bit:manyLower:2-10 tests/output/spec_ml_2_10.fa
	twoBitToFa tests/input/testMask.2bit tests/output/spec_ml_no_start_end.fa -seqList=tests/input/seqlist1
	twoBitToFa tests/input/testMask.2bit tests/output/spec_ml_partial_list.fa -seqList=tests/input/seqlist2
	twoBitToFa tests/input/testMask.2bit tests/output/bed_ml_partial_list.fa -bed=tests/input/seqlist2.bed
	diff -r -x CVS tests/expected tests/output
	rm -r tests/output
