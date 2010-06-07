struct seqList
/* Records data for one sequence in list. */
    {
    struct seqList *next;
    char *comment;      /* Fa file comment if any. */
    struct dnaSeq *seq; 
    int *softMask; /* One slogProb val for each base, lets us fade out individual bases if used. */
    int frame;     /* If coding this will be 0, 1, or 2 depending where in codon 1st base is. */
    };

