table strchive
"STRchive disease-associated short tandem repeat loci"
    (
    string chrom;              "Chromosome"
    uint chromStart;           "Start position"
    uint chromEnd;             "End position"
    string name;               "Locus ID (disease_gene)"
    uint score;                "Score (0-1000)"
    char[1] strand;            "Strand"
    uint thickStart;           "Thick start (same as chromStart)"
    uint thickEnd;             "Thick end (same as chromEnd)"
    uint reserved;             "Item color RGB"
    string gene;               "Gene symbol"
    string referenceMotif;     "Repeat motif in reference orientation"
    string pathogenicMotif;    "Pathogenic repeat motif"
    string pathogenicMin;      "Minimum pathogenic repeat count"
    string inheritance;        "Mode of inheritance"
    lstring disease;           "Associated disease(s)"
    )
