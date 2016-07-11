kentSrc = ../..
include ../../inc/common.mk


O = geneStarts.o 

L += $(MYSQLLIBS) -lm 
MYLIBDIR = /cse/guests/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a


geneStarts: $(O)
	${CC} ${COPT} ${CFLAGS} -o geneStarts $O $(MYLIBS) $L

clean::
	rm -f ${O}
