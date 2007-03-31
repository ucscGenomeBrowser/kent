table refCluster
"A cluster of overlapping refSeq."
    (
    string chrom;	"Chromosome"
    int chromStart;	"Chromosome start position"
    int chromEnd;	"Chromosome end position"
    string name;	"Cluster name"
    char[1] strand;	"Strand."
    int refCount;	"Number in cluster"
    string[refCount] refArray; "Array of accessions"
    )
