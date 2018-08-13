table txAliDoubleGap
"Double-sided gaps between reference genome assembly and transcript sequences"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Number of skipped bases on genome and transcript"
    char[1] strand;    "Transcript orientation on genome: + or -"
    string txName;     "Transcript identifier"
    uint   txStart;    "Start position in transcript"
    uint   txEnd;      "End position in transcript"
    uint   gSkipped;   "Number of bases skipped on genome, if any"
    uint   txSkipped;  "Number of bases skipped on transcript, if any"
    string hgvsCN;     "HGVS c./n. notation of part of transcript not matched in genome"
    )
