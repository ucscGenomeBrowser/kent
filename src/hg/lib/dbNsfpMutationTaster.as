table dbNsfpMutationTaster
"MutationTaster scores provided by dbNSFP (http://dbnsfp.houstonbioinformatics.org/)"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    enum('A','C','G','T') refAl;   "Allele found in reference assembly"
    lstring ensTxId;   "Ensembl transcript ID(s), if dbNSFP has data for >1 transcript set at this position; otherwise '.' to save space"
    enum('A','C','G','T') altAl1;       "alternate allele #1"
    lstring score1;                     "MutationTaster score for altAl1, or '.' if n/a"
    enum('D','N','A', 'P','.') pred1;   "MutationTaster prediction for altAl1: Damaging, Neutral, disease-causing Automatic, Polymorphism-automatic, not given"
    enum('A','C','G','T','.') altAl2;   "alternate allele #2"
    lstring score2;                     "MutationTaster score for altAl2, or '.' if n/a"
    enum('D','N','A', 'P','.') pred2;   "MutationTaster prediction for altAl2: Damaging, Neutral, disease-causing Automatic, Polymorphism-automatic, not given"
    enum('A','C','G','T','.') altAl3;   "alternate allele #3"
    lstring score3;                     "MutationTaster score for altAl3, or '.' if n/a"
    enum('D','N','A', 'P','.') pred3;   "MutationTaster prediction for altAl3: Damaging, Neutral, disease-causing Automatic, Polymorphism-automatic, not given"
    )
