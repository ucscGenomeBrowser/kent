include ../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

all:  raToLines linesToRa

raToLines: raToLines.o
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/raToLines raToLines.o $(MYLIBS) $L
	#${STRIP} ${BINDIR}/raToLines${EXE}

linesToRa: linesToRa.o
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/linesToRa linesToRa.o $(MYLIBS) $L
	#${STRIP} ${BINDIR}/linesToRa${EXE}

clean::
	rm -f raToLines.o linesToRa.o
