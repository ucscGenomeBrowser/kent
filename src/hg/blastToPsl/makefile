kentSrc = ../..
include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

blastToPsl_objs = blastToPsl.o pslBuild.o
blastXmlToPsl_objs = blastXmlToPsl.o ncbiBlast.o pslBuild.o

all: blastToPsl blastXmlToPsl

blastXmlToPsl: ${blastXmlToPsl_objs} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/blastXmlToPsl${EXE} ${blastXmlToPsl_objs} ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/blastXmlToPsl${EXE}

blastToPsl: ${blastToPsl_objs} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/blastToPsl${EXE} ${blastToPsl_objs} ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/blastToPsl${EXE}

compile: ${blastXmlToPsl_objs} ${blastToPsl_objs}
	${CC} ${COPT} -o blastXmlToPsl${EXE} ${blastXmlToPsl_objs} ${MYLIBS} $L
	${CC} ${COPT} -o blastToPsl${EXE} ${blastToPsl_objs} ${MYLIBS} $L

test:: compile
	cd tests && ${MAKE} test
	rm -fr tests/output blastXmlToPsl${EXE} blastToPsl${EXE}

clean::
	rm -fr tests/output
	rm -f ${blastToPsl_objs} ${blastXmlToPsl_objs}
	rm -f blastXmlToPsl${EXE} blastToPsl${EXE}

ncbiBlast.o: ncbiBlast.c ncbiBlast.h
pslBuild.o: pslBuild.c pslBuild.h
blastXmlToPsl.o: blastXmlToPsl.c ncbiBlast.h pslBuild.h
blastToPsl.o: blastToPsl.c pslBuild.h
