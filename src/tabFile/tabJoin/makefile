kentSrc = ../..
A = tabJoin
include $(kentSrc)/inc/userApp.mk

tst:
	$A	test1.tsv name test2.tsv lc test.out
	cmp expected.out test.out
