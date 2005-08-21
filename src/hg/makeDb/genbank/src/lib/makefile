GBROOT = ../..
include ${GBROOT}/common.mk

O = gbFileOps.o gbDefs.o gbIndex.o gbRelease.o gbGenome.o gbUpdate.o \
    gbProcessed.o gbEntry.o gbAligned.o uniqueStrTbl.o extFileTbl.o \
    gbStatusTbl.o sqlUpdater.o seqTbl.o gbFa.o imageCloneTbl.o \
    gbIgnore.o gbVerb.o sqlDeleter.o gbLoadedTbl.o mgcStatusTbl.o \
    gbPipeline.o gbConf.o gbSql.o

# python sources to copy to the lib
PY_SRCS = \
	py/genbank/Config.py \
	py/genbank/GbIndex.py \
	py/genbank/GenomeSeqs.py \
	py/genbank/Pipeline.py \
	py/genbank/__init__.py \
	py/genbank/fileOps.py \
	py/genbank/procOps.py

# files to compile into lib without compiling
LIBSRCS = gbCommon.pm gbFtp.pm gbCommon.sh ${PY_SRCS}
INSTLIBSRCS = ${LIBSRCS:%=${LIBDIR}/%}

all: ${LIBGENBANK} ${INSTLIBSRCS}

${LIBGENBANK}: $(O) 
	@${MKDIR} `dirname $@`
	ar rcu $@ $(O)
	ranlib $@

${LIBDIR}/%: %
	@${MKDIR} `dirname $@`
	cp -f $< $@
	chmod a-w,a+r $@

clean:
	rm -f $(O) ${LIBGENBANK} ${INSTLIBSRCS}

# lame attempt at dependencies
gbFileOps.o: ../inc/gbFileOps.h

gbIndex.o: ../inc/gbIndex.h ../inc/gbEntry.h ../inc/gbRelease.h 

gbGenome.o: ../inc/gbGenome.h ../inc/gbRelease.h

gbRelease.o: ../inc/gbRelease.h ../inc/gbGenome.h ../inc/gbIndex.h \
	../inc/gbUpdate.h ../inc/gbEntry.h \
	../inc/gbProcessed.h ../inc/gbAligned.h 

gbProcessed.o: ../inc/gbProcessed.h ../inc/gbRelease.h ../inc/gbUpdate.h \
	../inc/gbIndex.h ../inc/gbEntry.h ../inc/gbFileOps.h

gbUpdate.o: ../inc/gbUpdate.h ../inc/gbRelease.h ../inc/gbProcessed.h \
	../inc/gbAligned.h ../inc/gbGenome.h ../inc/gbEntry.h

gbAligned.o: ../inc/gbAligned.h ../inc/gbGenome.h ../inc/gbUpdate.h \
	../inc/gbIndex.h ../inc/gbRelease.h ../inc/gbEntry.h ../inc/gbFileOps.h

gbEntry.o: ../inc/gbEntry.h ../inc/gbGenome.h ../inc/gbRelease.h \
	../inc/gbAligned.h

gbSeqTbl.o: ../inc/gbSeqTbl.h
