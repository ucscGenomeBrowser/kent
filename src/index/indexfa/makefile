include ../../inc/common.mk


O = indexfa.o 

indexfa: $(O) lib
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/indexfa

lib:
	cd ../../lib;make; cd ../index/indexfa

