table mafFrames
"codon frame assignment for MAF components"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start range in chromosome"
    uint   chromEnd;   "End range in chromosome"
    string src;        "Name of sequence source in MAF"
    ubyte frame;       "frame (0,1,2) for first base(+) or last bast(-)"
    char[1] strand;    "+ or -"
    string name;       "Name of gene used to define frame"
    int    prevFramePos;  "target position of the previous base (in transcription direction) that continues this frame, or -1 if none, or frame not contiguous"
    int    nextFramePos;  "target position of the next base (in transcription direction) that continues this frame, or -1 if none, or frame not contiguous"
    ubyte  isExonStart;  "does this start the CDS portion of an exon?"
    ubyte  isExonEnd;    "does this end the CDS portion of an exon?"
    )
