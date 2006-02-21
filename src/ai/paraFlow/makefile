include ../../inc/common.mk

all:
	cd compiler && ${MAKE}
	cd runtime && ${MAKE}
	cd testProg && ${MAKE}

clean:
	rm -f */*.o
	cd testProg && ${MAKE} clean

test:
	cd testProg && ${MAKE}

q:
	cd testProg && ${MAKE} q

quickTest:
	cd testProg && ${MAKE} quickTest
