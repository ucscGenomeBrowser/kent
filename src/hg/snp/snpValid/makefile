kentSrc = ../../..
A = snpValid
include $(kentSrc)/inc/userApp.mk

test::
	${A} hg16 /cluster/data/hg16/bed/snp/build122/seq -chr=chr22 > hg16-chr22.log 

test2:
	${A} hg17 /cluster/data/hg17/bed/snp/build123/seq -chr=chr22 > hg17-chr22.log 


backup: clean
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip snpValid-%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

clean::
	rm -f *.log
	rm -f dump
