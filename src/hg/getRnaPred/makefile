include ../../inc/common.mk
HG_WARN_ERR = -DJK_WARN -Wall -Werror

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = getRnaPred.o

getRnaPred: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/getRnaPred $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/getRnaPred${EXE}
