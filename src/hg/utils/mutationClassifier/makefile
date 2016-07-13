kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

ifeq (${USE_BAM},1)
    MYLIBS += ${SAMLIB}
endif

A = mutationClassifier
O = ${A}.o

compile: $(O) $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O $(MYLIBS) $L

clean:
	rm -f $(O)

test:	FORCE
	mutationClassifier hg19 test/test.bed

FORCE:
	
