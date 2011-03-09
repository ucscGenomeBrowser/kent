include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = testCart.o cart.o

testCart: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o testCart $O $(MYLIBS) $L
	${STRIP} testCart${EXE}
	mv testCart /usr/local/apache/cgi-bin/testCart

clean::
	rm -f ${O}
