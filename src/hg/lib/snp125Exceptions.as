table snp125Exceptions
"Annotations on data from dbSNP (for version 125 and later)."
    (
    string  chrom;      "Reference sequence chromosome or scaffold"
    uint    chromStart; "Start position in chrom"
    uint    chromEnd;   "End position in chrom"
    string  name;       "Reference SNP identifier or Affy SNP name"
    string  exception;  "Exception found for this SNP"
    )
