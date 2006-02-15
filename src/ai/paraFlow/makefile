include ../../inc/common.mk

all:
	cd compiler && ${MAKE}
	cd runTime && ${MAKE}
	cd testProg && ${MAKE}

clean:
	rm -f */*.o
	cd testProg && ${MAKE} clean

test:
	cd testProg && ${MAKE}

quickTest:
	cd testProg && ${MAKE} quickTest
