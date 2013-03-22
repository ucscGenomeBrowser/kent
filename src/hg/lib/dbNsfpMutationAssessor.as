table dbNsfpMutationAssessor
"MutationAssessor scores provided by dbNSFP (http://dbnsfp.houstonbioinformatics.org/)"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    enum('A','C','G','T') refAl;   "Allele found in reference assembly"
    lstring ensTxId;   "Ensembl transcript ID(s), if dbNSFP has data for >1 transcript set at this position; otherwise '.' to save space"
    enum('A','C','G','T') altAl1;     "alternate allele #1"
    string score1;                    "MutationAssessor score for altAl1, or '.' if n/a"
    enum('high','low','medium', 'neutral','.') pred1;   "MutationAssessor prediction for altAl1"
    enum('A','C','G','T','.') altAl2; "alternate allele #2"
    string score2;                    "MutationAssessor score for altAl2, or '.' if n/a"
    enum('high','low','medium', 'neutral','.') pred2;   "MutationAssessor prediction for altAl2"
    enum('A','C','G','T','.') altAl3; "alternate allele #3"
    string score3;                    "MutationAssessor score for altAl3, or '.' if n/a"
    enum('high','low','medium', 'neutral','.') pred3;   "MutationAssessor prediction for altAl3"
    )
