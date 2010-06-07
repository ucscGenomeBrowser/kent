GBROOT = ../..
include ${GBROOT}/common.mk

O = mgcClone.o  mgcFullength.o  mgcLibrary.o  mgcStage1.o

PROGS = ${GB_BINARCH}/mgcImport ${GB_BINARCH}/mgcDbLoad \
	${GB_BINDIR}/mgcDownloadStep 


all: ${PROGS}

clean:
	rm -f ${O} ${PROGS}  mgcImport.o mgcDbLoad.o


${GB_BINARCH}/mgcImport:  mgcImport.o ${O} ${LIBGENBANK}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/mgcImport mgcImport.o ${O} $(LIBS)

${GB_BINARCH}/mgcDbLoad:  mgcDbLoad.o ${O} ${LIBGENBANK}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/mgcDbLoad mgcDbLoad.o ${O} $(LIBS)

