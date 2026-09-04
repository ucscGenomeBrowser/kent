table omimImprint
"Genes curated as imprinted by OMIM, from a GeneScout export"
    (
    string chrom;         "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart;    "Start position in chromosome"
    uint   chromEnd;      "End position in chromosome"
    string name;          "Gene symbol"
    uint   score;         "Score (unused, always 0)"
    char[1] strand;       "Strand, not given by OMIM, always ."
    uint   thickStart;    "Start of thick display"
    uint   thickEnd;      "End of thick display"
    uint   reserved;      "Color, neutral gray, see the description page"
    string mimNumber;     "MIM Number|OMIM identifier of the gene entry, empty when the gene has none"
    string cytoLocation;  "Cytogenetic Location|Band as given by OMIM"
    lstring geneName;     "Gene Name|Full gene name"
    string entryType;     "Entry Type|Whether OMIM has a gene entry for this locus"
    lstring phenotypes;   "Phenotypes|OMIM phenotypes listed alongside this gene in the export"
    lstring inheritance;  "Inheritance|Mode of inheritance of those phenotypes"
    )
