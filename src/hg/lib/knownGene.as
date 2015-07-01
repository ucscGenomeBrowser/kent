table knownGene
"Transcript from default gene set in UCSC browser"
(
string  name;               "Name of gene"
string  chrom;              "Reference sequence chromosome or scaffold"
char[1] strand;             "+ or - for strand"
uint    txStart;            "Transcription start position"
uint    txEnd;              "Transcription end position"
uint    cdsStart;           "Coding region start"
uint    cdsEnd;             "Coding region end"
uint    exonCount;          "Number of exons"
uint[exonCount] exonStarts; "Exon start positions"
uint[exonCount] exonEnds;   "Exon end positions"
string  proteinID;          "UniProt display ID, UniProt accession, or RefSeq protein ID" 
string  alignID;            "Unique identifier (GENCODE transcript ID for GENCODE Basic"
)

