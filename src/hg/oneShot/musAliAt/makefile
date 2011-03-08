include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = musAliAt.o

musAliAt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o musAliAt $O $(MYLIBS) $L

clean::
	rm -f ${O}
