table taxonGeneticCode 
"nbci genetic code table"
(
	uint    id; 			"genetic code id in GenBank taxonomy database"
 	string  abbr ;			"division code 3 characters "
 	string  name ;			"division name e.g. BCT, PLN, VRT, MAM, PRI..."
	string  tranlsation ;	        "translation table for this genetic code (one AA per codon 1-64)"
	string  starts ;			"start codons for this genetic code (one AA per codon 1-64)"
 	string  comments ;		        "free-text comments "
)

