kentSrc = ../..
include ../../inc/common.mk


L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgBatch.o

A = hgBatch

my: compile
	mv $A ${CGI_BIN}-${USER}/$A

alpha: compile
	mv $A ${CGI_BIN}/$A

beta: compile
	mv $A ${CGI_BIN}-beta/$A


compile: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $A $O $(MYLIBS) $L
	${STRIP} $A${EXE}
	chmod a+rx $A

install:	compile
	@if [ ! -d "${DESTDIR}${CGI_BIN}" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}"; \
	fi
	mv $A ${DESTDIR}${CGI_BIN}/$A

lib:
	cd ../lib && ${MAKE}
	cd ../../lib && ${MAKE}

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

clean::
	rm -f ${O}
