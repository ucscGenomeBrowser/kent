TABLE encodeHapMapAlleleFreq 
"HapMap SNPs in ENCODE regions"
(
    string chrom;		"Reference sequence chromosome or scaffold"
    uint chromStart;		"Start position"
    uint chromEnd;		"End position"
    string name;		"rsId or id"
    uint score;			"Minor allele frequency from 500 to 1000"
    char[1] strand;            	"Strand"
    string center;		"Sequencing center"
    char[1] refAllele;         	"Reference allele"
    char[1] otherAllele;	"Variant allele"
    float refAlleleFreq; 	"Reference allele frequency (between 0.0 and 1.0)"
    float otherAlleleFreq;	"Variant allele frequency (between 0.0 and 1.0)"
    float minorAlleleFreq;	"Smaller of the 2 frequencies (between 0.0 and 0.5)"
    uint totalCount;		"Count of individuals"
    float derivedAlleleFreq;	"Derived allele frequency"
)
