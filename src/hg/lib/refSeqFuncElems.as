table refSeqFuncElems
"Bed 6+ representation of the refSeq functional elements."
    (
    string chrom;          "Reference sequence chromosome or scaffold"
    uint   chromStart;     "Start position in chromosome"
    uint   chromEnd;       "End position in chromosome"
    string name;           "Numeric ID of the gene" 
    uint   score;          "unused; placeholder for BED format"
    char[1] strand;        "+ for forward strand, - for reverse strand"
    uint   thickStart;     "Start position in chromosome"
    uint   thickEnd;       "End position in chromosome"
    string soTerm;         "Sequence ontology (SO) term"
    lstring note;          "A note describing the element"
    lstring pubMedIds;     "PubMed ID (PMID) of associated publication(s)"
    lstring experiment;    "Experimental evidence"
    lstring function;      "Predicted function"
    string annotationID;   "Annotation release"
    lstring _mouseOver;    "Mouse over label"
    )
