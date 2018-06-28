
L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

%Test: %Test.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)
%: %.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)
	cp $@ /cluster/home/sugnet/slam

O = runSlam createSlamParaSpec prep_lc_rm_for_slam cutOutSeqs createCutOutSpec

TESTS = synMapTest

exe: $(O)

test: $(TESTS)

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f runSlam.o createSlamParaSpec.o prep_lc_rm_for_slam.o cutOutSeqs.o createCutOutSpec.o
