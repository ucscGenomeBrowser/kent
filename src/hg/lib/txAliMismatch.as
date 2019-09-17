table txAliMismatch
"Mismatches between reference genome assembly and transcript sequences"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Mismatching bases: transcript bases > genomic bases"
    char[1] strand;    "Transcript orientation on genome: + or -"
    string txName;     "Transcript identifier"
    uint   txStart;    "Start position in transcript"
    uint   txEnd;      "End position in transcript"
    lstring hgvsG;     "HGVS g. notation of genome change to match transcript"
    lstring hgvsCN;    "HGVS c./n. notation of transcript change to match genome"
    )
