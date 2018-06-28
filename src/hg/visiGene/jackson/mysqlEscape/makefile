kentSrc = ../../../..
include ../../../../inc/common.mk

JKSRCDIR = ../../../../../../kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)
XINC = -I$(MYSQLINC)

A = mysqlEscape

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBS = $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a 


all: $(A)


O = $(A).o

$(A): $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $L -o ${BINDIR}/$(A)
	${STRIP} ${BINDIR}/$(A)${EXE}
	chmod a+rx ${BINDIR}/$(A)${EXE}

backup:
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip -r $(EXE)%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

clean:
	rm -f *.o
	rm -f $(A)${EXE}

