include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

blastToPsl_objs = blastToPsl.o pslBuild.o
blastXmlToPsl_objs = blastXmlToPsl.o ncbiBlast.o pslBuild.o

all: blastToPsl blastXmlToPsl

compile: all

blastXmlToPsl: ${blastXmlToPsl_objs} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/blastXmlToPsl${EXE} ${blastXmlToPsl_objs} ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/blastXmlToPsl${EXE}

blastToPsl: ${blastToPsl_objs} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/blastToPsl${EXE} ${blastToPsl_objs} ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/blastToPsl${EXE}

test::
	(cd tests && ${MAKE} test)

clean::
	rm -f ${blastToPsl_objs} ${blastXmlToPsl_objs}

ncbiBlast.o: ncbiBlast.c ncbiBlast.h
pslBuild.o: pslBuild.c pslBuild.h
blastXmlToPsl.o: blastXmlToPsl.c ncbiBlast.h pslBuild.h
blastToPsl.o: blastToPsl.c pslBuild.h
