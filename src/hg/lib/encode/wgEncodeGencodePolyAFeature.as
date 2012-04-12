table wgEncodeGencodePolyAFeature
"GENCODE PolyA feature annotation in transcript and genome"
   (
    string transcriptId; "GENCODE transcript identifier"
    int transcriptStart; "Start position in transcript:"
    int transcriptEnd; "End position in transcript:"
    string chrom;  "Chromosome"
    uint chromStart; "Start position in chromosome"
    uint endStart; "End position in chromosome"
    char strand;  "Strand + or -"
    enum("polyA_signal", "polyA_site", "pseudo_polyA") feature; "Type of annotation"
   )
