include ../../inc/common.mk

L = $(MYSQLLIBS) -lm /usr/local/lib/libjson.a
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgData

O = $A.o hgData_index.o hgData_common.o hgData_response.o hgData_db.o hgData_genome.o hgData_track.o hgData_bed.o hgData_search.o hgData_maf.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}

.o:	hgData.h

