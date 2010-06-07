table pgSiftPred
"sift predictions for pgSnp tracks"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  prediction;     "sifts prediction, damaging, tolerated,..."
    string  geneId;	    "gene ID, Ensembl"
    string  geneName;	    "gene name"
    lstring geneDesc;	    "gene description"
    lstring protFamDesc;    "protein family description"
    string  omimDisease;    "OMIM disease"
    string  aveAlleleFreq;  "Average allele frequencies"
    string  ceuAlleleFreq;  "Caucasian allele frequencies"
    )
