kentSrc = ../..
A = pslLiftSubrangeBlat
preMyLibs += ../../lib/$(MACHTYPE)/jkhgap.a
include $(kentSrc)/inc/userApp.mk
HG_INC += -I../../hg/inc
L += $(MYSQLLIBS) -lm

test::
	cd tests && ${MAKE} test

clean::
	rm -fr tests/output
