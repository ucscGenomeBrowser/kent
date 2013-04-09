kentSrc = ../..
A = bedGeneParts
include $(kentSrc)/inc/userApp.mk
L += -lm

test::
	bedGeneParts firstExon test.in firstExon.out
	diff firstExon.expected firstExon.out
	bedGeneParts introns test.in introns.out
	diff introns.expected introns.out
	bedGeneParts promoter test.in promoter.out
	diff promoter.expected promoter.out
	bedGeneParts firstCodingSplice test.in firstCodingSplice.out
	diff firstCodingSplice.expected firstCodingSplice.out

clean::
	rm -f firstExon.out introns.out promoter.out firstCodingSplice.out
