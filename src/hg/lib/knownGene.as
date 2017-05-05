table knownGene
"Transcript from default gene set in UCSC browser"
(
string  name;               "Name of gene"
string  chrom;              "Reference sequence chromosome or scaffold"
char[1] strand;             "+ or - for strand"
uint    txStart;            "Transcription start position (or end position for minus strand item)"
uint    txEnd;              "Transcription end position (or start position for minus strand item)"
uint    cdsStart;           "Coding region start (or end position if for minus strand item)"
uint    cdsEnd;             "Coding region end (or start position if for minus strand item)"
uint    exonCount;          "Number of exons"
uint[exonCount] exonStarts; "Exon start positions (or end positions for minus strand item)"
uint[exonCount] exonEnds;   "Exon end positions (or start positions for minus strand item)"
string  proteinID;          "UniProt display ID, UniProt accession, or RefSeq protein ID" 
string  alignID;            "Unique identifier (GENCODE transcript ID for GENCODE Basic)"
)

