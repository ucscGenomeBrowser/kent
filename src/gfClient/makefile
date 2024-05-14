kentSrc = ..
A = gfClient
preMyLibs += ../lib/$(MACHTYPE)/jkOwnLib.a
include $(kentSrc)/inc/userApp.mk
L += -lm $(SOCKETLIB)

test::
	cd tests && ${MAKE} test

clean::
	cd tests && ${MAKE} clean
