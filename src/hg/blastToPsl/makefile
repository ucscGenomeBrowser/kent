include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = blastToPsl
O = $A.o 

my: compile
	mv $A ${CGI_BIN}-${USER}/$A

compile: $O 
	${CC} $O ${F64} ${MYLIBS} ${L} 
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

test:	compile
	cd test && ${MAKE}

clean:
	cd test && ${MAKE} clean
	rm -f $O $A

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
