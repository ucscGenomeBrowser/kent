kentSrc = ../..
include ../../inc/common.mk

L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

%Test: %Test.c 
	${CC} ${COPT}  -I../inc -I../../inc  -o $@ ${addsuffix .c,$@} ${MYLIBS} ${L}
%: %.c 
	${CC} ${COPT}  -I../inc -I../../inc  -o $@ ${addsuffix .c,$@} ${MYLIBS} ${L}
	mv $@ ${DESTDIR}${BINDIR}/

O = liftAgp

TESTS = 

exe: ${O}

test: ${TESTS}

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
