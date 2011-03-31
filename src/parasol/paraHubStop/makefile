include ../../inc/common.mk
L += -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraHubStop.o

paraHubStop: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraHubStop $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ../bin/paraHubStop${EXE}

clean::
	rm -f ${O}
