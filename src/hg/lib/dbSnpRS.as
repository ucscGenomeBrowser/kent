table dbSnpRS "Information from dbSNP at the reference SNP level"
    (
        uint    rsId;    "rs identifier"
        float   avHet;   "the average heterozygosity from all observations"
        float   avHetSE; "the Standard Error for the average heterozygosity from all observations"
        string  valid;   "the validation status of the SNP"
        char[1] base1;   "the base of the first allele"
        char[1] base2;   "the base of the second allele"
        string  assembly; "the sequence in the ucsc assembly"
        string  alternate; "the sequence of the alternate allele"
    )
