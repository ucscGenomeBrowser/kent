kentSrc = ../../../../..
include ../../../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = doOmimGene2.o

all: doOmimGene2

doOmimGene2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o doOmimGene2 $O $(MYLIBS) $L

clean::
	rm -f ${O}

install: check-prefix all
	cp doOmimGene2 ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
