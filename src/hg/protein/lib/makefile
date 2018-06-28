kentSrc = ../../..
XINC = -I${MYSQLINC}
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}

O = pbUtil.o doStamps.o doTracks.o domains.o spKgMap.o doSam.o uniref.o unirefTbl.o spMapper.o


../../../lib/${MACHTYPE}/jkpbap.a: ${O} 
	ar rcus ../../../lib/${MACHTYPE}/jkpbap.a ${O}

clean::
	rm -f ${O}
	rm -f ../../lib/${MACHTYPE}/jkpbap.a;

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

test:
	cd tests && ${MAKE} test
