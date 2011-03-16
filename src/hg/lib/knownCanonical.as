table knownCanonical
"Describes the canonical splice variant of a gene"
    (
    string chrom;	"Chromosome"
    int chromStart;	"Start position (0 based). Corresponds to txStart"
    int chromEnd;	"End position (non-inclusive). Corresponds to txEnd"
    int clusterId;	"Which cluster of transcripts this belongs to in knownIsoforms"
    string transcript;	"Corresponds to knownGene name field."
    string protein;	"SwissProt ID or more recently UCSC ID of associated protein."
    )
