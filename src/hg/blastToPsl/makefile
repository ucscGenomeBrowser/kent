include ../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

blastToPsl_objs = blastToPsl.o pslBuild.o
blastXmlToPsl_objs = blastXmlToPsl.o ncbiBlast.o pslBuild.o

all: blastToPsl blastXmlToPsl

compile: all

%: %.o ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/$*${EXE} ${$*_objs} ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/$*${EXE}

test::
	(cd tests && ${MAKE} test)

clean::
	rm -f ${blastToPsl_objs} ${blastXmlToPsl_objs}

ncbiBlast.o: ncbiBlast.c ncbiBlast.h
pslBuild.o: pslBuild.c pslBuild.h
blastXmlToPsl.o: blastXmlToPsl.c ncbiBlast.h pslBuild.h
blastXmlToPsl: ${blastXmlToPsl_objs}
blastToPsl: ${blastToPsl_objs}
