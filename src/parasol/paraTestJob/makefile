kentSrc = ../..
include ../../inc/common.mk

#L += -lm -lz
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a ../../htslib/libhts.a -lpthread -lssl -lcrypto -lm -lz

O = paraTestJob.o

paraTestJob: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraTestJob $O $(MYLIBS)
	${STRIP} ../bin/paraTestJob${EXE}

clean::
	rm -f ${O}

print:
	@echo "MYLIBS $(MYLIBS)"
	@echo "L      $(L)	   "
