include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}
L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraHubStop.o

paraHubStop: $O $(MYLIBS)
	gcc -o ../bin/paraHubStop $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	${STRIP} ../bin/paraHubStop${EXE}
