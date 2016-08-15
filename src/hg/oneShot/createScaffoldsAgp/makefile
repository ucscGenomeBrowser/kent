kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = createScaffoldsAgp.o

createScaffoldsAgp: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/createScaffoldsAgp $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/createScaffoldsAgp${EXE}

clean:
	rm -f $O
