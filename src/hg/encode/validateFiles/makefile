include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = validateFiles.o

validateFiles: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/validateFiles $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/validateFiles${EXE}

clean:
	rm -f $O

test: 	validateFiles
	rm -rf output
	mkdir output
	-validateFiles -type=tagAlign -genome=/cluster/data/hg18/hg18.2bit  tests/test1.txt > output/test1.out  2>&1 
	cmp output/test1.out tests/test1.right.txt
	-validateFiles -nMatch -type=tagAlign -genome=/cluster/data/hg18/hg18.2bit  tests/test2.txt > output/test2.out  2>&1 
	cmp output/test2.out tests/test2.right.txt
	validateFiles  -type=BAM -chromDb=hg18  tests/test3.bam > output/test3.out  2>&1 
	cmp output/test3.out tests/test3.right.txt
	validateFiles  -type=bigWig -chromDb=hg18  tests/test4.bw > output/test4.out  2>&1 
	cmp output/test4.out tests/test4.right.txt
