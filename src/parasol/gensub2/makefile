include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = gensub2.o

gensub2: $O $(MYLIBS)
	${MKDIR} ../bin
	gcc -ggdb -o ../bin/gensub2 $O $(MYLIBS) $L
	${STRIP} ../bin/gensub2${EXE}
