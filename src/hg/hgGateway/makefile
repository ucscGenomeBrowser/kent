include ../../inc/common.mk


L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgGateway

O = $A.o

my: compile
	mv $A ${CGI_BIN}-${USER}/$A

alpha: compile
	mv $A ${CGI_BIN}/$A

beta: compile
	mv $A ${CGI_BIN}-beta/$A

compile: $O 
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A
	strip $A
	chmod a+rx $A

install:	compile
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
