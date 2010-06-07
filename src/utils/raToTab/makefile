include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raToTab.o

raToTab: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/raToTab $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/raToTab${EXE}

clean:
	rm -f $O

test:
	raToTab in.ra out.tab -head
	diff expected.tab out.tab
	raToTab in.ra -cols=name,function out2.tab
	diff expected2.tab out2.tab
