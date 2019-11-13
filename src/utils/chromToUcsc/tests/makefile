all:
	mkdir -p out
	python2 `which chromToUcsc` -i in/hg19.bed -o out/hg19.bed -g hg19 -a hg19.chromAlias.tsv
	python2 `which chromToUcsc` -i in/mm10.bed -o out/mm10.bed -a mm10.chromAlias.tsv
	python3 `which chromToUcsc` -i in/hg19.bed -o out/hg19.bed -g hg19 -a hg19.chromAlias.tsv
	python3 `which chromToUcsc` -i in/mm10.bed -o out/mm10.bed -a mm10.chromAlias.tsv
	diff out/hg19.bed expected/hg19.bed
	diff out/mm10.bed expected/mm10.bed
