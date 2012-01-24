table knownCanonical
"Describes the canonical splice variant of a gene"
    (
    string chrom;	"Chromosome"
    int chromStart;	"Start position (0 based). Corresponds to txStart for + strand genes"
    int chromEnd;	"End position (non-inclusive). Corresponds to txEnd for + strand genes"
    int clusterId;	"Which cluster of transcripts this belongs to in knownIsoforms"
    string transcript;	"Corresponds to knownGene name field."
    string protein;	"UniProt ID of the associated protein, if any."
    )
