kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgBlatTest.o

hgBlatTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgBlatTest $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/hgBlatTest${EXE}

test:
	hgBlatTest hgwdev-${USER}.cse.ucsc.edu/cgi-bin/hgBlat test.log org=human db=hg16 type=DNA sort=score output=psl repeat=1 

test2:
	hgBlatTest hgwbeta.cse.ucsc.edu/cgi-bin/hgBlat test.log org=chimp type=DNA sort=score output=psl repeat=1 

test3:
	hgBlatTest hgwdev.cse.ucsc.edu/cgi-bin/hgBlat test.log type=DNA sort=score output=psl repeat=1 

backup: clean
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip hgBlatTest%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

clean:
	rm -f $O
	rm -f *.log
