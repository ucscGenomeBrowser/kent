table avada
"AVADA variant information"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Used as itemRgb as of 2004-11-22"
    string pmid;       "Pubmed ID"
    string geneSym;       "Gene Symbol"
    string variant;     "Variant as in Paper"
    string ensId;       "Gene (Ensembl)"
    string entrezs;       "Gene (Entrez)"
    string refSeq;       "Transcript (RefSeq)"
    )
