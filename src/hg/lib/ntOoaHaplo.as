table ntOoaHaplo
"Candidate regions for gene flow from Neandertal to non-African modern humans (Table 5 of Green RE et al., Science 2010)"
    (
    string chrom;      "Reference sequence chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Qualitative assessment (OOA = out of africa, COS = cosmopolitan)"
    uint   score;      "For BED compatibility: Score from 0-1000 (placeholder = 0)"
    char[1] strand;    "For BED compatibility: + or - (placeholder = +)"
    uint thickStart;   "For BED compatibility: Start of where display should be thick"
    uint thickEnd;     "For BED compatibility: End of where display should be thick"
    uint reserved;     "itemRgb color code"
    float  st;         "Estimated ratio of OOA/African gene tree depth"
    float  ooaTagFreq; "Average frequency of tag in OOA clade"
    ubyte  am;          "Neandertal (M)atches OOA-specific clade (Ancestral)"
    ubyte  dm;          "Neandertal (M)atches OOA-specific clade (Derived)"
    ubyte  an;          "Neandertal does (N)ot match OOA-specific clade (Ancestral)"
    ubyte  dn;          "Neandertal does (N)ot match OOA-specific clade (Derived)"
    )
