include ../inc/common.mk

O = ameme.o fragFind.o

alpha: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a -lm 
	${STRIP} a.out${EXE}
	mv a.out $(HOME)/bin/$(MACHTYPE)/ameme 

cgi: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out ameme.exe
	chmod a+rx ameme.exe
	mv ameme.exe $(HOME)/.html/cgi-bin


