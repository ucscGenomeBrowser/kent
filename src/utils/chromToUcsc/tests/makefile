all:
	mkdir -p out
	python2 `which chromToUcsc` -i in/hg19.bed -o out/hg19.bed -g hg19
	python2 `which chromToUcsc` -a mm10.chromAlias.tsv -i in/mm10.bed -o out/mm10.bed	
	python3 `which chromToUcsc` -i in/hg19.bed -o out/hg19.bed -g hg19
	python3 `which chromToUcsc` -a mm10.chromAlias.tsv -i in/mm10.bed -o out/mm10.bed	
	diff out/hg19.bed expected/hg19.bed
	diff out/mm10.bed expected/mm10.bed
