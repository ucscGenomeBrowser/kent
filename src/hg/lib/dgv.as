table dgv
"Database of Genomic Variants"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Name of item"
    uint   score;       "Score from 0-1000"
    char[1] strand;     "+ or -"
    uint thickStart;    "Start of where display should be thick (start codon)"
    uint thickEnd;      "End of where display should be thick (stop codon)"
    uint itemRgb;	"Item R,G,B color."
    string landmark;    "Genomic marker near the variation locus"
    string varType;     "Type of variation"
    string reference;   "Literature reference for the study that included this variant"
    uint pubMedId;      "For linking to pubMed abstract of reference"
    string method;      "Brief description of method/platform"
    lstring sample;     "Description of sample population for the study"
    )
