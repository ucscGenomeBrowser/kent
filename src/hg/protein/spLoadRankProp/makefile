include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS = $(MYLIBDIR)/jkpbap.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spLoadRankProp.o rankPropProt.o

$(HOME)/bin/$(MACHTYPE)/spLoadRankProp: $O $(MYLIBS)
	${CC} ${CFLAGS} -o $@ $O $(MYLIBS) $L

clean::
	rm -f $O
