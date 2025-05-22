table bedMethyl
"Browser extensible data for bedmethyl files (bed9+9)"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Used as itemRgb as of 2004-11-22"
    string nValidCov;       "Valid Coverage|N_mod + N_otherMod + N_canonical"
    string percMod;       "Percent Modified"
    string nMod;       "N_mod|Number of calls with a modified base"
    string nCanon;       "N_canonical|Number of calls with a canonical base"
    string nOther;       "N_otherMod|Number of calls with a modified base, other modification"
    string nDelete;       "N_delete|Number of reads with a deletion at this reference position"
    string nFail;       "N_fail|Number of calls where the probability of the call was below the threshold. "
    string nDiff;       "N_diff|Number of reads with a base other than the canonical base for this modification. "
    string nNoCall;       "N_nocall|Number of reads aligned to this reference position, with the correct canonical base, but without a base modification call."
    )

