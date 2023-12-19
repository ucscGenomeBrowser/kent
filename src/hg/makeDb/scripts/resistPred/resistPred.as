table resistPredj
"Browser extensible data (9 fields) plus other mutation annotations"
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
    # Count   Mutation Type   Annotation Type Base change:Virus number        AA change       Residue number in PDB
    string count;       "Frequency in Mar 2022"
    string mut;       "Mutation Type"
    string annot;       "Annotations Type"
    lstring changeFreq;       "Base change and frequency for each"
    lstring aa;       "AA change"
    lstring res;       "Residue number in PDB structure"
    )
