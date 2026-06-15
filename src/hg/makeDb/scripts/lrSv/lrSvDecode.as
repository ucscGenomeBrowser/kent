table lrSvDecode
"deCODE Icelandic long-read structural variants"
    (
    string chrom;       "Chromosome"
    uint chromStart;    "Start position"
    uint chromEnd;      "End position"
    string name;        "Variant ID"
    uint score;         "Score"
    char[1] strand;     "Strand"
    uint thickStart;    "Thick start (same as chromStart)"
    uint thickEnd;      "Thick end (same as chromEnd)"
    uint reserved;      "Item color"
    string svType;      "SV Type|DEL, INS, or INSDEL (combined insertion/deletion)"
    int svLen;          "SV Length|Length of the variant on the reference in base pairs"
    int insLen;         "Insertion Length|Length of inserted sequence, 0 for DEL/INV/CPX"
    string AC;          "Allele Count|Not published by deCODE for this site-only callset"
    string trrBegin;    "Tandem Repeat Region Start|Start of the surrounding tandem repeat region, if any (TRRBEGIN)"
    string trrEnd;      "Tandem Repeat Region End|End of the surrounding tandem repeat region, if any (TRREND)"
    )
