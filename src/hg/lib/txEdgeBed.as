table txEdgeBed
"A transcription edge with first fields bed format."
    (
    string chrom;	"Chromosome or contig name"
    int chromStart;	"Start position, zero-based"
    int chromEnd;	"End position, non-inclusive"
    string name;	"Name of evidence supporting edge"
    int score;		"Score - 0-1000"
    char[1] strand;	"Strand - either plus or minus"
    char[1] startType;	"[ or ( for hard or soft"
    enum(exon, intron) type; "edge type"
    char[1] endType;	"] or ) for hard or soft"
    )
