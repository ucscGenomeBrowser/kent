include ../../inc/common.mk


L += -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = uniqSize.o 

uniqSize: ${O}
	${CC} ${COPT} -o uniqSize $O ${MYLIBS} ${L} 

clean::
	rm -f uniqSize ${O}
