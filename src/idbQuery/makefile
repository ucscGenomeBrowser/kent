include ../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = idbQuery.o 

#idbQuery.exe: $(O)
#	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
#	mv a.out idbQuery.exe
#	chmod a+rx idbQuery.exe
#	mv idbQuery.exe ${CGI_BIN}-${USER}

idbQuery: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/idbQuery${EXE} $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/idbQuery${EXE}
