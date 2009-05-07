XINC = -I${HOME}/include/
include ../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a ${HOME}/lib/libjson.a

A = hgData

O = $A.o hgData_index.o hgData_common.o hgData_response.o hgData_db.o hgData_genome.o hgData_track.o hgData_bed.o hgData_search.o hgData_maf.o hgData_wig.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

.o:	hgData.h

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

test: test.o
	${CC} test.o ${MYLIBS} ${L}
	mv ${AOUT} test
