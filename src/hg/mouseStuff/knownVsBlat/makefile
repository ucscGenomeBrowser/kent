include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

S = blatStats.o

O = knownVsBlat.o $S

all: knownVsBlat kvsSummary

knownVsBlat: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/knownVsBlat $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/knownVsBlat${EXE}

O2 = kvsSummary.o $S

kvsSummary: $(O2) $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/kvsSummary $(O2) $(MYLIBS) $L
	${STRIP} ${BINDIR}/kvsSummary${EXE}

clean::
	rm -f ${O}
