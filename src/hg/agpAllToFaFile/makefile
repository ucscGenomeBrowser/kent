include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpAllToFaFile.o

agpAllToFaFile: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/agpAllToFaFile $O $(MYLIBS) $L

clean::
	rm -f ${O}
