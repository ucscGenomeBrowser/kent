include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS= -g


all : kgName fbName 

kgName: $(MYLIBS) kgName.o
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/kgName kgName.o $(MYLIBS) $L
fbName: $(MYLIBS) fbName.o
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fbName fbName.o $(MYLIBS) $L
#	${STRIP} ${BINDIR}/kgName${EXE}

clean::
	rm -f kgName.o fbName.o
