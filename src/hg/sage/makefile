
L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(L)

O = addAveMedScoreToPsls  createSageSummary


%: %.c 
	gcc  -ggdb -Wimplicit  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS)
	cp $@ $(HOME)/bin/$(MACHTYPE)/

all : $(O)

#$(O) : %: %.c ../lib/$(ARCHTYPE)/libsugnet.a 
#	gcc -Wimplicit -ggdb  -I../inc -I$(KENT_DIR)/inc -I$(KENT_DIR)/hg/inc -o $@ $(addsuffix .c,$@)  $(SUGNET_LIBS) $(KENT_LIBS)

clean :
	rm -f $(O)
	rm -rf $(ARCHTYPE) 
