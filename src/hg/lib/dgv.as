table dgv
"Database of Genomic Variants"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Name of item"
    uint   score;       "Score from 0-1000"
    char[1] strand;     "+ or -"
    uint thickStart;    "Same as chromStart (placeholder for BED 9+ format)"
    uint thickEnd;      "Same as chromEnd (placeholder for BED 9+ format)"
    uint itemRgb;	"Item R,G,B color."
    lstring landmark;    "Genomic marker near the variation locus"
    string varType;     "Type of variation"
    string reference;   "Literature reference for the study that included this variant"
    uint pubMedId;      "For linking to pubMed abstract of reference"
    lstring method;      "Brief description of method/platform"
    lstring sample;     "Description of sample population for the study"
    )
