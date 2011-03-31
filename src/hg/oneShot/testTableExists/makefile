include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = testTableExists
O = $A.o

testTableExists: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/$A $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/$A${EXE}

clean::
	rm -f $O

