include ../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = dnadust.o 

dnadust.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out dnadust.exe
	chmod a+rx dnadust.exe
	mv dnadust.exe ${CGI_BIN}-${USER}

