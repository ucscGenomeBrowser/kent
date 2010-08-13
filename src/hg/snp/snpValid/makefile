include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = snpValid
O = ${A}.o

${A}: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

test:
	${A} hg16 /cluster/data/hg16/bed/snp/build122/seq -chr=chr22 > hg16-chr22.log 

test2:
	${A} hg17 /cluster/data/hg17/bed/snp/build123/seq -chr=chr22 > hg17-chr22.log 


backup: clean
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip snpValid-%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

clean:
	rm -f $O
	rm -f $A
	rm -f *.log
	rm -f dump
