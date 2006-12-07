table cnpRedon
"CNP data from Redon lab"
    (
    string  chrom;      "Reference sequence chromosome or scaffold"
    uint    chromStart; "Start position in chrom"
    uint    chromEnd;   "End position in chrom"
    string  name;       "Reference SNP identifier or Affy SNP name"
    float   score;      "Score"
    char[1] strand;     "Strand"
    )
