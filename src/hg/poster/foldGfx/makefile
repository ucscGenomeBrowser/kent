include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = foldGfx.o

foldGfx: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o foldGfx $O $(MYLIBS) $L

lib:
	cd ../lib; make
	cd ../../lib; make
