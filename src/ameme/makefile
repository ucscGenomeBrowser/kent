include ../inc/common.mk
O = ameme.o fragFind.o
CFLAGS = -DGS_PATH='${GS_PATH}'
# set location of gs on webserver with: make GS_PATH='"/full_path"' cgi

alpha: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a -lm 
	${STRIP} a.out${EXE}
	mv a.out $(HOME)/bin/$(MACHTYPE)/ameme

cgi: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a -lm 
	mv a.out ameme.exe
	chmod 755 ameme.exe
	mv ameme.exe $(HOME)/.html/cgi-bin


