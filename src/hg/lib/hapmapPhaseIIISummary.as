table hapmapPhaseIIISummary
"HapMap Phase III allele summaries for filtering (BED 5+)"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom (0 based)"
    uint    chromEnd;   "End position in chrom (1 based)"
    string  name;       "Reference SNP identifier from dbSnp"
    uint    score;      "Average of population heterozygosities in parts per thousand (0-500)"

    string  observed;            "Observed string from genotype file"
    char    overallMajorAllele;  "This allele has been observed in at least half of the populations that have data for this SNP"
    char    overallMinorAllele;  "This allele may not have been observed in any HapMap sample"
    ubyte   popCount;            "How many Phase III populations have data (1-11)"
    ubyte   phaseIIPopCount;     "How many Phase II populations have data (0-4)"
    ubyte   isMixed;             "0 if all populations have the same major allele, 1 otherwise."
    ubyte[11] foundInPop;        "Got data for each of the 11 Phase III populations?"
    ubyte[11] monomorphicInPop;  "Monomorphic in each of the 11 Phase III populations?"
    float   minFreq;             "Minimum minor allele frequency across all populations"
    float   maxFreq;             "Maximum minor allele frequency across all populations"

    uint     orthoCount;           "Species for which orthologous alleles have been determined"
    char[orthoCount] orthoAlleles; "Orthologous allele for each species (or N if not found)"
    ushort[orthoCount] orthoQuals; "Base quality score (0-100) for each species"
    )
