table wgEncodeGencodePolyAFeature
"GENCODE PolyA feature annotation in transcript and genome"
   (
    string transcriptId;     "GENCODE transcript identifier"
    int transcriptStart;     "Start position in transcript:"
    int transcriptEnd;       "End position in transcript:"
    string chrom;            "Chromosome"
    uint chromStart;         "Start position in chromosome"
    uint chromEnd;           "End position in chromosome"
    char[1] strand;          "+ or - or . for unknown"
    enum("polyA_signal", "polyA_site", "pseudo_polyA") feature; "Type of annotation"
   )
