table sage
"stores sage data in terms of uni-gene identifiers."
(
	int uni; "Number portion of uni-gene identifier, add 'Hs.' for full identifier."
	char[64] gb; "Genebank accesion number."
	char[64] gi; "gi field in unigene descriptions."
	lstring description; "Description from uni-gene fasta headers."
	int numTags; "Number of tags."
	string[numTags] tags; "Tags for this unigene sequence."
	int numExps; "Number of experiments."
	int[numExps] exps; "index of experiments in order of aves and stdevs."
	float[numExps] meds; "The median count of all tags for each experiment."
	float[numExps] aves; "The average count of all tags for each experiment."
	float[numExps] stdevs; "Standard deviation of all counts for each experiment"
)

