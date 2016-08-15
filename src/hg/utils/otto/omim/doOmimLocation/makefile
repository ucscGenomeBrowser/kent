kentSrc = ../../../../..
include ../../../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = doOmimLocation.o

all: doOmimLocation

doOmimLocation: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o doOmimLocation $O $(MYLIBS) $L

clean::
	rm -f ${O}

install: check-prefix all
	cp doOmimLocation ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
