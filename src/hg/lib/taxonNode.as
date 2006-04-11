table taxonNode 
"ncbi taxonomy node tree"
(
	uint    taxon ;			"node id in GenBank taxonomy database"
 	uint    parent ;		"parent node id in GenBank taxonomy database"
 	string  rank;			"rank of this node (superkingdom, kingdom, ...) "
 	string  emblcode;		"locus-name prefix; not unique"
 	uint    division;               "ncbiDivision id (0=Bacteria, 2=Mammal, 5=Primate, 6=Rodent, 10=Vertabrate...)"
 	uint    inheritedDiv;	        "1 if node inherits division from parent"
 	uint    geneticCode;     	"genetic code used by species, see ncbiGencode"
 	uint    inheritedGC;	        "1 if node inherits genetic code from parent"
 	uint    mitoGeneticCode;   	"genetic code of mitochondria see ncbiGencode"
 	uint    inheritedMitoGC;      	"1 if node inherits mitochondrial gencode from parent"
 	uint    GenBankHidden;          "1 if name is suppressed in GenBank entry lineage"
 	uint    notSequenced;           "1 if this subtree has no sequence data yet"
 	string  comments;		"free-text comments and citations"
)
