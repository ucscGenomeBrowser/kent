
L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)
%: %.c 
	gcc -static -Wimplicit -ggdb  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)
	cp $@ /cluster/home/sugnet/slam

O = runSlam createSlamParaSpec prep_lc_rm_for_slam

TESTS = synMapTest

exe: $(O)

test: $(TESTS)

lib:
	cd ../../lib; make
