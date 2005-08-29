include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


H = cDnaAligns.h overlapFilter.h polyASize.h usage.h
O = cDnaAligns.o overlapFilter.o polyASize.o pslCDnaFilter.o
B =  ${HOME}/bin/${MACHTYPE}/pslCDnaFilter

$B: pslCDnaFilter
	@${MKDIR} `dirname $B`
	cp -f pslCDnaFilter $B

pslCDnaFilter: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o pslCDnaFilter $O ${MYLIBS} ${MYSQLLIBS} $L

pslCDnaFilter.o: pslCDnaFilter.c $H
cDnaAligns.o: cDnaAligns.c $H
overlapFilter.o: overlapFilter.c $H
polyASizes.o: polyASizes.c polyASizes.h

# create usage msg from text
usage.h: usage.txt
	awk '!/^#/{print "\"" $$0 "\\n\""}' $< > $@

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O pslCDnaFilter usage.h
	cd tests && ${MAKE} clean

savebak:
	savebak pslCDnaFilter *.c *.h *.as usage.txt makefile tests/makefile tests/input

