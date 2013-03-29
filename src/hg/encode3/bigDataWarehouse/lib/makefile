include ../../../../inc/common.mk

O = bigDataWarehouse.o bdwLib.o

${MACHTYPE}/bdwlib.a: ${O}
	mkdir -p ${MACHTYPE}
	ar rcu ${MACHTYPE}/bdwlib.a ${O}
	ranlib ${MACHTYPE}/bdwlib.a

clean::
	rm -f ${O}
