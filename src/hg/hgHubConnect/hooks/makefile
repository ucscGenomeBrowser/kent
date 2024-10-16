kentSrc = ../../..
include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

COMMONO = hooklib.o
HOOKEXE = pre-create post-finish

all: install

pre-create: ${COMMONO}  pre-create.o
	${CC} -o pre-create -g pre-create.o ${MYLIBS} ${L} ${COMMONO}

post-finish: ${COMMONO} post-finish.o
	${CC} -o post-finish -g post-finish.o ${MYLIBS} ${L} ${COMMONO}

compile: pre-create post-finish

install:: compile
	mv ${HOOKEXE} /cluster/home/chmalee/tusd/tusd_linux_amd64/hooks/

alpha::
	@echo 'would run make alpha'

clean:
	rm -f *.o ${HOOKEXE}
