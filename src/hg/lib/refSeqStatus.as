table refSeqStatus
"RefSeq Gene Status."
    (
    string mrnaAcc;	"RefSeq gene accession name"
    enum('Unknown','Reviewed','Validated','Provisional','Predicted','Inferred') status;	"Status of RefSeq"
    enum ('DNA', 'RNA', 'ds-RNA', 'ds-mRNA', 'ds-rRNA', 'mRNA', 'ms-DNA', 'ms-RNA', 'rRNA', 'scRNA', 'snRNA', 'snoRNA', 'ss-DNA', 'ss-RNA', 'ss-snoRNA', 'tRNA') mol; "molecule type"
    )
