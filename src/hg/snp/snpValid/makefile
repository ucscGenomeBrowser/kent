include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

X = snpValid
O = ${X}.o

${X}: $O ${MYLIBS}
	${CC} ${COPT} -o ${X} $O ${MYLIBS} $L
	${STRIP} ${X}${EXE}
	cp ${X} ${HOME}/bin/${MACHTYPE}/${X} 

test:
	${X} hg16 /cluster/data/hg16/bed/snp/build122/seq -chr=chr22 > hg16-chr22.log 

test2:
	${X} hg17 /cluster/data/hg17/bed/snp/build123/seq -chr=chr22 > hg17-chr22.log 


backup: clean
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip snpValid-%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

clean:
	rm -f $O
	rm -f $X
	rm -f *.log
	rm -f dump
