include ../../inc/common.mk


L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgc

O = $A.o mafClick.o

my: compile
	mv $A ${CGI_BIN}-${USER}/$A

mgsc: compile
	mv $A ${CGI_BIN}-mgsc/$A

alpha: strip
	mv $A ${CGI_BIN}/$A

beta: strip
	mv $A ${CGI_BIN}-beta/$A

strip:  compile
	strip $A

compile: $O 
	${CC} $O ${F64} ${MYLIBS} ${L} 
	mv a.out $A
	chmod a+rx $A

install:	compile
	strip $A
	@if [ ! -d "${DESTDIR}${CGI_BIN}" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}"; \
	fi
	mv $A ${DESTDIR}${CGI_BIN}/$A

debug: $O
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A


lib:
	cd ../../lib; make

clean:
	rm -f $O $A

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
