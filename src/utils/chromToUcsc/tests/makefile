all:
	mkdir -p out
	chromToUcsc -i in/hg19.bed -o out/hg19.bed -g hg19 2> /dev/null
	chromToUcsc -a mm10.chromAlias.tsv -i in/mm10.bed -o out/mm10.bed 2> /dev/null
	diff out/hg19.bed expected/hg19.bed
	diff out/mm10.bed expected/mm10.bed
