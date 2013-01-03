table coriellDelDup
"Coriell Cell Line Deletions and Duplications"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Short Name of item"
    uint   score;      "score == (CN state * 100) -> (0,100,200,300,400)"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "color from CN state"
    enum("0", "1", "2", "3", "4") CN_State;   "CN State"
    enum("B_Lymphocyte", "Fibroblast", "Amniotic_fluid_cell_line", "Chorionic_villus_cell_line") cellType;   "Cell Type"
    string description;  "Description (Diagnosis)"
    string ISCN;  "ISCN nomenclature"
    )
