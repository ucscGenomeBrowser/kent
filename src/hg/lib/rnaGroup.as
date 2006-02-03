table rnaGroup
"Details of grouping of rna in cluster"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Always 1000"
    char[1] strand;    "+ or -"
    uint refSeqCount;	"Number of refSeq mrnas in cluster"
    string[refSeqCount] refSeqs;	"List of refSeq accessions"
    uint genBankCount;	"Number of Genbank mrnas in cluster"
    string[genBankCount] genBanks;	"List of Genbank accessions"
    uint rikenCount;	"Number of Riken mrnas in cluster"
    string[rikenCount] rikens;		"List of Riken IDs"
    )
