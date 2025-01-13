kentSrc = ../../..

include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ../lib/$(MACHTYPE)/libSpliceGraph.a 

% : %.c
	${CC} ${COPT} ${CFLAGS} -Wimplicit -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) ${L} $(MYSQLLIBS)
	cp $@ ${BINDIR}

O =  axtCountBeds

exe: $(O)

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
