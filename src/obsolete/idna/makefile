include ../../inc/common.mk


O = idna.o 

idna.cgi: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out idna.cgi
	chmod a+rx idna.cgi
	mv idna.cgi ../cgi-bin
