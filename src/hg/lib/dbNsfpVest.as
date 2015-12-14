table dbNsfpVest
"VEST scores provided by dbNSFP (http://dbnsfp.houstonbioinformatics.org/)"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    enum('A','C','G','T','.') refAl;  "Allele found in reference assembly"
    lstring ensTxId;   "Ensembl transcript ID(s)"
    lstring vestTxId;  "ID of transcript on which VEST prediction was made"
    enum('A','C','G','T') altAl1;     "Alternate allele #1"
    lstring var1;                     "Protein change(s) caused by altAl1"
    lstring score1;                   "VEST score(s) for altAl1, or '.' if n/a"
    enum('A','C','G','T') altAl2;     "Alternate allele #2"
    lstring var2;                     "Protein change(s) caused by altAl2"
    lstring score2;                   "VEST score(s) for altAl2, or '.' if n/a"
    enum('A','C','G','T') altAl3;     "Alternate allele #3"
    lstring var3;                     "Protein change(s) caused by altAl3"
    lstring score3;                   "VEST score(s) for altAl3, or '.' if n/a"
    )
