kentSrc = ../..
A = tagStormFromTab
include $(kentSrc)/inc/userApp.mk

test::
	tagStormFromTab in.tab out.tags
	diff expected.tags out.tags
