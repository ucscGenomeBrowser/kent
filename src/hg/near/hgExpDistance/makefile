include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

hgExpDistance: hgExpDistance.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance $@.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance${EXE}

hgExpDistance_fm: hgExpDistance_fm.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance_fm $@.o ${MYLIBS} $L -lpthread
	${STRIP} ${BINDIR}/hgExpDistance_fm${EXE}

hgExpDistance_fq: hgExpDistance_fq.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance_fq $@.o ${MYLIBS} $L -lpthread
	${STRIP} ${BINDIR}/hgExpDistance_fq${EXE}

hgExpDistance_tm: hgExpDistance_tm.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance_tm $@.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance_tm${EXE}

hgExpDistance_fm_tm: hgExpDistance_fm_tm.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance_fm_tm $@.o ${MYLIBS} $L -lpthread
	${STRIP} ${BINDIR}/hgExpDistance_fm_tm${EXE}

hgExpDistance_fq_tm: hgExpDistance_fq_tm.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance_fq_tm $@.o ${MYLIBS} $L -lpthread
	${STRIP} ${BINDIR}/hgExpDistance_fq_tm${EXE}

diffDistTables: diffDistTables.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/diffDistTables $@.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/diffDistTables${EXE}

time: hgExpDistance_tm.o ${MYLIBS}
	hgExpDistance_tm hg17 gnfAtlas2 gnfAtlas2Exp gnfAtlas2_ORIG_TEST  -verbose=2

time_fm: hgExpDistance_fm_tm.o ${MYLIBS}
	hgExpDistance_fm_tm hg17 gnfAtlas2 gnfAtlas2Exp gnfAtlas2_FM_TEST  -verbose=2

time_fq: hgExpDistance_fq_tm.o ${MYLIBS}
	hgExpDistance_fq_tm hg17 gnfAtlas2 gnfAtlas2Exp gnfAtlas2_FQ_TEST  -verbose=2

test_fm: hgExpDistance_fm.o diffDistTables.o ${MYLIBS}
	hgExpDistance_fm hg17 gnfAtlas2 gnfAtlas2Exp gnfAtlas2_FM_TEST
	diffDistTables hg17 gnfAtlas2_FM_TEST gnfAtlas2_ORIG_TEST

test_fq: hgExpDistance_fq.o diffDistTables.o ${MYLIBS}
	hgExpDistance_fq hg17 gnfAtlas2 gnfAtlas2Exp gnfAtlas2_FQ_TEST
	diffDistTables hg17 gnfAtlas2_FQ_TEST gnfAtlas2_ORIG_TEST

clean::
	rm -f hgExpDistance*.o 
