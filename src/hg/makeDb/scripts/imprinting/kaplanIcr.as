table kaplanIcr
"Imprinting control regions with boundaries revised by fragment-level methylation (Rosenski et al. 2025)"
    (
    string  chrom;       "Reference sequence chromosome or scaffold"
    uint    chromStart;  "Start position in chromosome"
    uint    chromEnd;    "End position in chromosome"
    string  name;        "ICR name, gene and the part of the gene it covers"
    uint    score;       "Not used, always 0"
    char[1] strand;      "Not applicable, always ."
    uint    thickStart;  "Start of where display should be thick"
    uint    thickEnd;    "End of where display should be thick"
    uint    itemRgb;     "Colour by the gamete in which the methylation mark is laid down"
    string  icrType;     "Type|germline DMR methylated in the oocyte or in sperm, or a DMR that acquires its methylation after fertilisation"
    lstring genes;       "Associated genes|imprinted genes linked to this control region in the literature"
    string  parentalAsm; "Parent-of-origin ASM|whether this study recovered the region as a parent-of-origin allele-specific methylation region"
    string  origCoords;  "Previous boundaries|the interval this control region had before the revision, lifted to this assembly"
    string  liftNote;      "Lifting note|set when hg38 inserted sequence inside the region, so that its boundaries no longer match the published hg19 ones"
    )
