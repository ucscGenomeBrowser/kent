table clinvarSubBB
"ClinVar variant submission info plus some fields from clivarMain"
    (
    string chrom;        "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart;   "Start position in chromosome"
    uint   chromEnd;     "End position in chromosome"
    string name;         "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Used as itemRgb as of 2004-11-22"

# from clinvarMain
    lstring origName;         "Link to ClinVar with Variant ID"
    string subGeneSymbol; "SubmittedGeneSymbol"
    string molConseq;         "Molecular Consequence"
    string snpId;         "dbSNP ID"

# from clinvarSub
    int varId;   "Variation ID"
    string clinSign; "Clinical Significance"
    string reviewStatus;   "Variant Review Status"
    string dateLastEval;  "Date Last Evaluated"
    lstring description; "Description"
    lstring subPhenoInfo; "Submitted Phenotype Info"
    lstring repPhenoInfo; "Reported Phenotype Info"
    string revStatus; "Submission Review Status"
    string collMethod;"Collection Method"
    string originCounts; "Origin Counts"
    string submitter; "Submitter"
    string scv; "SCV"
    lstring explOfInterp; "Explanation of Interpretation"
    )
