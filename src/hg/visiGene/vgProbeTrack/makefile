kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgProbeTrack.o

vgProbeTrack: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgProbeTrack $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/vgProbeTrack${EXE}

autoSql:
	autoSql vgPrb.as vgPrb -dbLink
	sed -i -e 's/    id int not null/    id int auto_increment not null/' vgPrb.sql
	sed -i -e 's/seq longblob/seq longtext/' vgPrb.sql
	autoSql vgPrbMap.as vgPrbMap -dbLink
	sed -i -e 's/PRIMARY KEY(probe)//' vgPrbMap.sql
	sed -i -e 's/\,	\# ID of vgPrb//' vgPrbMap.sql
	autoSql vgPrbAli.as vgPrbAli -dbLink
	sed -i -e 's/PRIMARY KEY(db)/PRIMARY KEY(db,vgPrb)/' vgPrbAli.sql
	autoSql vgPrbAliAll.as vgPrbAliAll -dbLink
	sed -i -e 's/PRIMARY KEY(db)/PRIMARY KEY(db,vgPrb)/' vgPrbAliAll.sql

clean:
	rm -f $O

