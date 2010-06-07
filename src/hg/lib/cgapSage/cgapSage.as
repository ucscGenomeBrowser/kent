table cgapSage
"Mappings and frequencies for CGAP SAGE tags"
(
        string  chrom;          "Reference sequence chromosome or scaffold"
        uint    chromStart;     "Start in Chromosome"
        uint    chromEnd;       "End in Chromosome"
        string  name;           "Name"
        uint    score;          "Score"
        char[1] strand;         "Strand"
	uint    thickStart;     "Thick start"
  	uint    thickEnd;       "Thick end"
	uint    numLibs;        "Number of libraries with data for this tag"
	uint[numLibs] libIds;   "Ids of libraries (foreign keys)"
	uint[numLibs] freqs;    "Frequency of each tag per library"
	double[numLibs] tagTpms;  "Tag per million measurement of each lib"
	uint	numSnps;	"Number of class=single SNPs in this region"
	string[numSnps] snps;	"List of SNPs"
)
