table wgEncodeGencodeExonSupport
"GENCODE exon support from other datasets"
   (
    string transcriptId; "GENCODE transcript identifier"
    string seqId; "Identifier of sequence supporting transcript"
    string seqSrc; "Source of supporting sequence"
    string exonId; "GENCODE exon identifier (not stable)"
    string chrom;  "chromosome"
    int chromStart; "start in chromosome"
    int endStart; "end in chromosome"
   )
