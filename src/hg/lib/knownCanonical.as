table knownCanonical
"Describes the canonical splice variant of a gene"
    (
    string chrom;	"Chromosome"
    int chromStart;	"Start position (0 based). Represents transcription start for + strand genes, end for - strand genes"
    int chromEnd;	"End position (non-inclusive). Represents transcription end for + strand genes, start for - strand genes"
    int clusterId;	"Which cluster of transcripts this belongs to in knownIsoforms"
    string transcript;	"Corresponds to knownGene name field."
    string protein;	"Accession of the associated protein, or UCSC ID in newer tables."
    )
