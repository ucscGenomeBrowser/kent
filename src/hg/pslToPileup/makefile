include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

A = pslToPileup
O = $(A).o 

$(A): $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/$(A) $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/$(A)${EXE}

lib:
	cd ../../lib && ${MAKE}
