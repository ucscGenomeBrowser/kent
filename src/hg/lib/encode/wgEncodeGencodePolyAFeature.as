table wgEncodeGencodePolyAFeature
"GENCODE PolyA feature annotation in transcript and genome"
   (
    string transcriptId; "GENCODE transcript identifier"
    int transcriptStart; "starting position in transcript:"
    int transcriptEnd; "ending position in transcript:"
    string chrom;  "chromosome"
    int chromStart; "start in chromosome"
    int endStart; "end in chromosome"
    char strand;  "strand"
    enum("polyA_signal", "polyA_site", "pseudo_polyA") feature; "type of annotation"
   )
