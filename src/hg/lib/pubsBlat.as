table pubsBlat
"publications blat feature table, in bed12+ format, additional field seqIds and seqRanges"
    (
    string chrom;         "chromosome"
    int chromStart;       "start position on chromosome"
    int chromEnd;         "end position on chromosome"
    string name;          "internal articleId, article that matches here"
    int score;            "score of feature"
    char[1] strand;       "strand of feature"
    int thickStart;       "start of exons"
    int thickEnd;         "end of exons"
    int reserved;         "no clue"
    int blockCount;       "number of blocks"
    lstring blockSizes;   "size of blocks"
    lstring chromStarts;  "A comma-separated list of block starts"
    string tSeqTypes;     "comma-seq list of matching sequence db (g=genome, p=protein, c=cDNA)"
    lstring seqIds;       "comma-separated list of matching seqIds"
    lstring seqRanges;    "ranges start-end on sequence that matched, one for each seqId"
    )
