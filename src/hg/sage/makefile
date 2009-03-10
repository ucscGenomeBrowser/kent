include ../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(L)

O = addAveMedScoreToPsls  createSageSummary


%: %.c 
	${CC} ${COPT} ${CFLAGS}  -Wimplicit  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS)
	cp $@ ${BINDIR}/

all : $(O)

#$(O) : %: %.c ../lib/$(ARCHTYPE)/libsugnet.a 
#	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I$(KENT_DIR)/inc -I$(KENT_DIR)/hg/inc -o $@ $(addsuffix .c,$@)  $(SUGNET_LIBS) $(KENT_LIBS)

clean :
	rm -f $(O)
	rm -rf $(ARCHTYPE) 
