include ../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
#COPT = -ggdb

A = pslQuickFilter

O = $A.o

compile: $O 
	gcc $O ${MYLIBS} ${L} 
	mv a.out $A
	${STRIP} $A${EXE}
	chmod a+rx $A
	cp $A $$HOME/bin/${MACHTYPE}

debug: $O
	gcc $O ${MYLIBS} ${L} 
	mv a.out $A

lib:
	cd ../../lib; make

tags:  
	ctags *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
