include ../../inc/common.mk


L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = checkHgFindSpec
O = $A.o 

all: compile install
compile: $A

$A: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $A $O $(MYLIBS) $L

install: compile
	mv $A ${BINDIR}/$A

clean::
	rm -f ${O}
