include ../../inc/common.mk


O = deEns.o 

deEns: $(O)
	${CC} ${COPT} ${CFLAGS} -o deEns $O ../../lib/$(MACHTYPE)/jkweb.a ${L}


lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
