include ../../inc/common.mk
HG_WARN += ${HG_WERR}

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


H = selectTable.h
O = selectTable.o overlapSelect.o
B =  $(HOME)/bin/$(MACHTYPE)/overlapSelect

$B: overlapSelect
	@mkdir -p `dirname $B`
	cp -f overlapSelect $B

overlapSelect: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o overlapSelect $O $(MYLIBS) ${MYSQLLIBS} $L


test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B
	cd tests && ${MAKE} clean
