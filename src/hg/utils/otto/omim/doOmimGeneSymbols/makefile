kentSrc = ../../../../..
include ../../../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = doOmimGeneSymbols.o

all: doOmimGeneSymbols

doOmimGeneSymbols: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o doOmimGeneSymbols $O $(MYLIBS) $L

clean::
	rm -f ${O}

install: check-prefix all
	cp doOmimGeneSymbols ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
