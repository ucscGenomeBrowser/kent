kentSrc = ../../..
include ../../../inc/common.mk


L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkQa.o 

checkQa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o checkQa $O $(MYLIBS) $L


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
