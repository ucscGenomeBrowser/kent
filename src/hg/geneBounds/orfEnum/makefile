kentSrc = ../../..
A = orfEnum
include $(kentSrc)/inc/userApp.mk

test::
	orfEnum in.fa out.orf
	diff expected.orf out.orf
	orfEnum -various in.fa outVarious.orf
	diff expectedVarious.orf outVarious.orf

clean:
	rm -f out.orf outVarious.orf
