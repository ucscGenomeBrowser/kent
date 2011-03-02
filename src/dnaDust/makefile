include ../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = dnadust.o 

dnadust.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L}
	mv ${AOUT} dnadust.exe
	chmod a+rx dnadust.exe

push:
	mv dnadust.exe ${CGI_BIN}-${USER}


clean::
	rm -f ${O} dnadust.exe ${AOUT}
