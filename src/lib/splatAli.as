table splatAli
"A parsed out splt format alignment."
    (
    string chrom;	"Chromosome mapped to"
    int chromStart;	"Start position in chromosome (zero based)"
    int chromEnd;	"End position in genome (one based)"
    string alignedBases;"Tag bases - in upper case for match, -/^ for insert/delete"
    int score;		"Mapping score. 1000/placesMapped"
    char[1] strand;     "+ or - for strand"
    string readName;	"Name of read"
    )

