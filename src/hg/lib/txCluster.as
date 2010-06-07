table txCluster
"A cluster of transcripts or coding regions"
    (
    string chrom;	"Chromosome (or contig)"
    int chromStart;	"Zero based start within chromosome."
    int chromEnd;	"End coordinate, non inclusive."
    string name;	"Cluster name"
    int score;		"BED score - 0-1000.  0 if not used."
    char[1] strand;	"Strand - either plus or minus"
    int txCount;	"Count of transcripts"
    string[txCount] txArray; "Array of transcripts"
    )
