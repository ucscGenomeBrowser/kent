
include ../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

% : %.c
	$(CC) $(COPT) -I../inc  -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) ${L} $(MYSQLLIBS) -lm -lz
	cp $@ ${BINDIR}

O = affyUclaMergePslData affyPslAndAtlasToBed pslAffySelect


exe: $(O)

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f affyPslAndAtlasToBed affyUclaMergePslData

