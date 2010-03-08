table pgPolyphenPred
"polyphen predictions for pgSnp tracks"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  prediction;     "polyphens prediction, damaging, benign,..."
    string  basedOn;	    "prediction basis"
    string  geneName;	    "gene name"
    string  aaChange;       "amino acid change"
    )
