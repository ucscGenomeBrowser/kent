include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslPseudo.o 

pslPseudo: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslPseudo $O $(MYLIBS) $L
	#strip $(HOME)/bin/$(MACHTYPE)/pslPseudo

lib:
	cd ../../lib; make

test: test1 test2 test3 test4 test5 test8 test9 test10 test11

test1: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test1.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test1g.psl output/test1p.psl  
	diff expected/test1g.psl output/test1g.psl 
	diff expected/test1p.psl output/test1p.psl
test2: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test2.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test2g.psl output/test2p.psl  
	diff expected/test2g.psl output/test2g.psl 
	diff expected/test2p.psl output/test2p.psl
test3: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test3.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test3g.psl output/test3p.psl  
	diff expected/test3g.psl output/test3g.psl 
	diff expected/test3p.psl output/test3p.psl
test4: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test4.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test4g.psl output/test4p.psl  
	diff expected/test4g.psl output/test4g.psl 
	diff expected/test4p.psl output/test4p.psl
test5: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test5.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test5g.psl output/test5p.psl  
	diff expected/test5g.psl output/test5g.psl 
	diff expected/test5p.psl output/test5p.psl
test6: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test6.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test6g.psl output/test6p.psl  
	diff expected/test6g.psl output/test6g.psl 
	diff expected/test6p.psl output/test6p.psl
test7: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test7.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test7g.psl output/test7p.psl  
	diff expected/test7g.psl output/test7g.psl 
	diff expected/test7p.psl output/test7p.psl
test8: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test8.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test8g.psl output/test8p.psl  
	diff expected/test8g.psl output/test8g.psl 
	diff expected/test8p.psl output/test8p.psl
test9: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test9.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test9g.psl output/test9p.psl  
	diff expected/test9g.psl output/test9g.psl 
	diff expected/test9p.psl output/test9p.psl
test10: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test10.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test10g.psl output/test10p.psl  
	diff expected/test10g.psl output/test10g.psl 
	diff expected/test10p.psl output/test10p.psl
test11: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test11.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test11g.psl output/test11p.psl  
	diff expected/test11g.psl output/test11g.psl 
	diff expected/test11p.psl output/test11p.psl
testRpl31: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/testRpl31.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/testRpl31g.psl output/testRpl31p.psl  
	diff expected/testRpl31g.psl output/testRpl31g.psl 
	diff expected/testRpl31p.psl output/testRpl31p.psl
test12: mkout
	pslPseudo -sizeMatters -minAli=0.98 -nearTop=0.005 input/test12.psl /cluster/store5/gs.16/build33/chrom.sizes /cluster/bluearc/hg/gs.16/build33/rmsk  /cluster/store5/gs.16/build33/bed/blastz.mm3.2003-04-12-03-MS/axtSyn/mouseSynBed.bed /cluster/store5/gs.16/build33/bed/simpleRepeat/simpleRepeat.bed output/test12g.psl output/test12p.psl  
	diff expected/test12g.psl output/test12g.psl 
	diff expected/test12p.psl output/test12p.psl



mkout:
	mkdir -p output
