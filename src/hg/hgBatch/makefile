include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
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
	gcc -ggdb -o $A $O $(MYLIBS) $L
	strip $A
	chmod a+rx $A

install:	compile
	@if [ ! -d "${DESTDIR}${CGI_BIN}" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}"; \
	fi
	mv $A ${DESTDIR}${CGI_BIN}/$A

lib:
	cd ../lib; make
	cd ../../lib; make

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
