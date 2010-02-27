include ../../inc/common.mk


L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = checkTableCoords
O = $A.o 

all: compile install

compile: $A

$A: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $A $O $(MYLIBS) $L

install: compile
	${STRIP} $A${EXE}
	mv $A ${BINDIR}/$A

test:
	cd tests && ${MAKE}

clean:
	-rm -f $A *.o
	cd tests && ${MAKE} clean

