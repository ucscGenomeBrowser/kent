table dbNsfpSift
"SIFT scores provided by dbNSFP (http://dbnsfp.houstonbioinformatics.org/)"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    enum('A','C','G','T') refAl;   "Allele found in reference assembly"
    lstring ensTxId;   "Ensembl transcript ID(s), if dbNSFP has data for >1 transcript set at this position; otherwise '.' to save space"
    enum('A','C','G','T') altAl1;     "alternate allele #1"
    lstring score1;     "SIFT score for altAl1 (or '.' if n/a): < 0.05 is 'Damaging', otherwise 'Tolerated'"
    enum('A','C','G','T','.') altAl2; "alternate allele #2"
    lstring score2;     "SIFT score for altAl2 (or '.' if n/a): < 0.05 is 'Damaging', otherwise 'Tolerated'"
    enum('A','C','G','T','.') altAl3; "alternate allele #3"
    lstring score3;     "SIFT score for altAl3 (or '.' if n/a): < 0.05 is 'Damaging', otherwise 'Tolerated'"
    )
