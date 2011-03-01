include ../../inc/common.mk


O = xaShow.o 

xaShow.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} xaShow.exe
	chmod a+rx xaShow.exe
	chmod g-w xaShow.exe
	mv xaShow.exe $(HOME)/.html/cgi-bin

clean::
	rm -f ${O}
