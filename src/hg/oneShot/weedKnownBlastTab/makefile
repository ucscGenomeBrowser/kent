include ../../../inc/common.mk

L += -lm $(MYSQLLIBS) 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = weedKnownBlastTab.o

weedKnownBlastTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o weedKnownBlastTab $O $(MYLIBS) $L
	${STRIP} weedKnownBlastTab${EXE}

clean::
	rm -f ${O}
