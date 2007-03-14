table refSeqStatus
"RefSeq Gene Status."
    (
    string mrnaAcc;	"RefSeq gene accession name"
    string status;	"Status (Reviewed, Provisional, Predicted)"
    enum ('DNA', 'RNA', 'ds-RNA', 'ds-mRNA', 'ds-rRNA', 'mRNA', 'ms-DNA', 'ms-RNA', 'rRNA', 'scRNA', 'snRNA', 'snoRNA', 'ss-DNA', 'ss-RNA', 'ss-snoRNA', 'tRNA') mol; "molecule type"
    )
