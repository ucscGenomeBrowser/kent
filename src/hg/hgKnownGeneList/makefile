include ../../inc/common.mk


MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a -lm /usr/lib/mysql/libmysqlclient.a -lz 

O = hgKnownGeneList.o

hgKnownGeneList: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgKnownGeneList $O $(MYLIBS)
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgKnownGeneList${EXE}
