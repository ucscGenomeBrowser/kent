table humanPhenotype 
"track for human phenotype data from locus specific databases"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "HGVS description of mutation."
    string  dbId;           "Db key.ID from source for this variant."
    string  linkDbs;        "Database keys which can link to this variant, comma sep."
    string  baseChangeType; "insertion, deletion, substitution,duplication,complex,unknown."
    string  location;       "intron, exon, 5'utr, 3'utr, not within known transcription unit."
    )

table humPhenLink 
"links for human phenotype detail page"
    (
    string linkDb;          "Database (key) of variants in humanPhenotype table."
    string linkDisplayName; "Display name for this link."
    string url;             "url to substitute ID in for links."
    )

table humPhenAlias
"aliases for mutations in the human phenotype track"
    (
    string dbId;            "ID from humanPhenotype table."
    string name;            "Another name for the variant."
    )

