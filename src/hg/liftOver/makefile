include ../../inc/common.mk


L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = liftOver.o

all: liftOver liftOverMerge

liftOver: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/liftOver $O $(MYLIBS) $L

liftOverMerge:  liftOverMerge.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/liftOverMerge liftOverMerge.o $(MYLIBS) $L

test::
	cd tests && ${MAKE} test
