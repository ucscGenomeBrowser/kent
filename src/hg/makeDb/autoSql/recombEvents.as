# # Chr (chromosome)
# lbnd (lower edge of crossover event in GRCh38 coordinates)
# ubnd (upper edge of crossover event in GRCh38 coordinates)
# pid (proband ID)
# ptype (parent type as P:paternal or M:maternal) 
# medsnp (physical position of median location of crossover in GRCh38 coordinates)
# complex (indicates if crossover is complex; 1:complex, 0:non-complex, NA:not tested)
# Chr     lbnd    ubnd    pid     ptype   medsnp  complex

table recombEvents
"Browser extensible data (12 fields) plus two sequences"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    #uint   score;      "Score from 0-1000"
    #char[1] strand;    "+ or -"
    #uint thickStart;   "Start of where display should be thick (start codon)"
    #uint thickEnd;     "End of where display should be thick (stop codon)"
    #uint reserved;     "Used as itemRgb as of 2004-11-22"
    #int blockCount;    "Number of blocks"
    #int[blockCount] blockSizes; "Comma separated list of block sizes"
    #int[blockCount] chromStarts; "Start positions relative to chromStart"
    #string seq1;       "sequence 1, normally left paired end"
    #string seq2;       "sequence 2, normally right paired end"
    string ptype; "Parent Type (P=Paternal, M=Maternal)"
    string medsnp; "Position of median location of crossover"
    string complex; "1:complex, 0:non-complex, NA:not tested"
    )
