GBROOT = ../..
include ${GBROOT}/common.mk

O = mgcClone.o  mgcFullength.o  mgcLibrary.o  mgcStage1.o

PROGS = ${BINARCH}/mgcImport ${BINARCH}/mgcDbLoad \
	${BINDIR}/mgcDownloadStep 


all: ${PROGS}

clean:
	rm -f ${O} ${PROGS}  mgcImport.o mgcDbLoad.o


${BINARCH}/mgcImport:  mgcImport.o ${O} ${LIBGENBANK}
	@mkdir -p ${BINARCH}
	gcc ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/mgcImport mgcImport.o ${O} $(LIBS)

${BINARCH}/mgcDbLoad:  mgcDbLoad.o ${O} ${LIBGENBANK}
	@mkdir -p ${BINARCH}
	gcc ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/mgcDbLoad mgcDbLoad.o ${O} $(LIBS)

