include ../inc/common.mk

L += -lm -lz
MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = ameme.o fragFind.o
ifdef GS_PATH
CFLAGS = -DGS_PATH='${GS_PATH}'
endif
# set location of gs on webserver with: make GS_PATH='"/full_path"' cgi

alpha: $(O)
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L}
	${STRIP} ${AOUT}${EXE}
	mv ${AOUT} ${BINDIR}/ameme

cgi: $(O)
	${CC} ${COPT} ${CFLAGS} $O ${MYLIBS} ${L}
	mv ${AOUT} ameme.exe
	chmod 755 ameme.exe
	mv ameme.exe $(HOME)/.html/cgi-bin

clean::
	rm -f ${O} ${AOUT} ameme.exe

