
tThrowback:
	blat -verbose=0 throwback/target1.fa throwback/query1.fa throwback/test.psl
	pslCheck throwback/test.psl
	blat -verbose=0 v29skips/ex1_database.fa v29skips/ex1_query.fa v29skips/ex1.psl
	diff v29skips/ex1_reference.psl v29skips/ex1.psl
	blat -verbose=0 v29skips/ex2_database.fa v29skips/ex2_query.fa v29skips/ex2.psl
	diff v29skips/ex2_reference.psl v29skips/ex2.psl
#	blat -verbose=0 /cluster/data/mm5/nib/chrX.nib badFine/badFine.fa -ooc=$HOME/.html/exe/data/mouse11.ooc badFine/badFine_reference.psl -fine
