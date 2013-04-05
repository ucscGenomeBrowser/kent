kentSrc = ../..
A = scaffoldFaToAgp
preMyLibs += $(kentSrc)/lib/$(MACHTYPE)/jkhgap.a
include $(kentSrc)/inc/userApp.mk

test:: ${A}
	cd test && csh runtest
