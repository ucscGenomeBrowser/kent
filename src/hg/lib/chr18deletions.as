table chr18deletions
"Chromosome 18 deletion"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of STS marker"
    uint score;	        "Score of a marker = 1000/(# of placements)"
    char[1] strand;	"Strand = or -"
    uint ssCount;	"Number of small blocks"
    uint[ssCount] smallStarts;	"Small start positions"
    uint seCount;	"Number of small blocks"
    uint[seCount] smallEnds;	"Small end positions"
    uint lsCount;	"Number of large blocks"
    uint[lsCount] largeStarts;	"Large start positions"
    uint leCount;	"Number of large blocks"
    uint[leCount] largeEnds;    	"Large end positions"
    )
