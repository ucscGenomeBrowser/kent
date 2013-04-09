kentSrc = ../..
A = spacedToTab
include $(kentSrc)/inc/userApp.mk
L += -lm

test::
	spacedToTab in.txt out.tab
	diff expected.tab out.tab
	spacedToTab in.txt -sizes=17,21,22 out2.tab
	diff expected2.tab out2.tab
	rm -f out.tab out2.tab

clean::
	rm -f out.tab out2.tab
