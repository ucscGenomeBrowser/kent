include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tailLines.o 
A = tailLines

${A}: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O $(MYLIBS) ${L}

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O} ${A}
