
include ../../../inc/common.mk

NIBDIR=/cluster/data/hg15/nib

test: liver heart

clean:
	rm -f testLiver.psl testHeart.psl
	
liver: liver.fa liver.tab BLOSUM45 liver.psl $(NIBDIR) 
	rm -f testLiver.psl 
	../blastToPsl liver.tab $(NIBDIR) liver.fa BLOSUM45 testLiver.psl
	-@cmp liver.psl testLiver.psl 

heart: heart.fa heart.tab BLOSUM45 heart.psl $(NIBDIR) 
	rm -f testHeart.psl
	../blastToPsl heart.tab $(NIBDIR) heart.fa BLOSUM45 testHeart.psl
	-@cmp heart.psl testHeart.psl
