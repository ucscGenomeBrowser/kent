include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = validateManifest.o

validateManifest: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/validateManifest $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/validateManifest${EXE}

clean:
	rm -f $O

test: 	validateManifest
	validateManifest -dir=/hive/groups/encode/encode3/testSets/manifest1

# note "-" before the command means you know it will have an error, but continue making
