kentSrc = ../..
A = raToTab
include $(kentSrc)/inc/userApp.mk
L += -lm

clean::
	rm -f out.tab out2.tab

test::
	${A} in.ra out.tab -head
	diff expected.tab out.tab
	${A} in.ra -cols=name,function out2.tab
	diff expected2.tab out2.tab
