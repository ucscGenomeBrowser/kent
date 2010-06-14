include ../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = idbQuery.o 

idbQuery.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} idbQuery.exe
	chmod a+rx idbQuery.exe

push:
	mv idbQuery.exe ${CGI_BIN}-${USER}

clean::
	rm -f ${O} idbQuery.exe ${AOUT}
