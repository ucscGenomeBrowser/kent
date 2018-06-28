kentSrc = ../..
include ../../inc/common.mk

HG_WARN=-Wall

#L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ../../htslib/libhts.a ${MYSQLLIBS} -lmysqlclient -lpthread -lz -lm -lssl -lcrypto -ldl -lstdc++ -lrt

A = cartReset

O = $A.o

include ../../inc/cgi_build_rules.mk

compile : $O
	${CC} ${CFLAGS} $O -o ${A}${EXE} ${MYLIBS}
	#mv $@dbinary $A${EXE}


print:
	@echo "MYLIBS $(MYLIBS)"
	@echo "L      $(L)	   "
	@echo "AOUT   $(AOUT)	   "
