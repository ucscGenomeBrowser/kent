include ../inc/common.mk
O = ameme.o fragFind.o
ifdef GS_PATH
CFLAGS = -DGS_PATH='${GS_PATH}'
endif
# set location of gs on webserver with: make GS_PATH='"/full_path"' cgi

alpha: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a -lm 
	${STRIP} ${AOUT}${EXE}
	mv ${AOUT} ${BINDIR}/ameme

cgi: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a -lm 
	mv ${AOUT} ameme.exe
	chmod 755 ameme.exe
	mv ameme.exe $(HOME)/.html/cgi-bin


