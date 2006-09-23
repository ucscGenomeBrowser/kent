include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = $(MYSQLLIBS) -lm -lz
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGenericMicroarray
O = ${A}.o

compile: $(O) $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/${A} $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/${A}${EXE}

clean:
	rm -f $(O)
