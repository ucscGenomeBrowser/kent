table snp125CodingCoordless
"Annotations of the effects of SNPs on translated protein sequence."
    (
    string      name;         "Reference SNP identifier"
    string      transcript;   "Accession of the transcript modified by SNP"
    enum(1,2,3) frame;        "Frame of transcript's codon modified by SNP"
    int         alleleCount;  "Number of annotated alleles for SNP"
    ushort[alleleCount]  funcCodes; "dbSNP function codes (e.g. 42=missense)"
    string[alleleCount]  alleles;   "Observed nucleotide alleles corresponding to funcCodes"
    lstring[alleleCount] codons;    "Codons corresponding to funcCodes"
    lstring[alleleCount] peptides;  "Amino acid sequence corresponding to funcCodes"
    )
