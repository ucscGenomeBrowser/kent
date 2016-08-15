kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raFromTab.o

raFromTab: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/raFromTab $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/raFromTab${EXE}

test:
	raFromTab in.tab cols=first,last,occupation out1.ra
	diff expected1.ra out1.ra
	raFromTab in.tab as=in.as out2.ra
	diff expected2.ra out2.ra

clean:
	rm -f $O
