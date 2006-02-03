table easyGene
"BED-style EasyGene format"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Name of item"
    uint score;         "Score"
    char[1] strand;     "Strand"
    string feat;        "Feature identifier"
    float R;            "R value"
    uint frame;         "Frame"
    string orf;         "ORF identifier"
    string startCodon;  "Start Codon"
    float logOdds;      "Log odds"
    string descriptor;  "EasyGene descriptor"
    string swissProt;   "Swiss-Prot match"
    char[1] genbank;    "Y or N this is the genbank one"
    )
