
table cdsEvidence
"Evidence for CDS boundary within a transcript"
    (
    string name;	"Name of transcript"
    int start;		"CDS start within transcript, zero based"
    int end;		"CDS end, non-inclusive"
    string source;	"Source of evidence"
    string accession;	"Genbank/uniProt accession"
    double score;	"0-1000, higher is better"
    ubyte startComplete;"Starts with ATG?"
    ubyte endComplete;	"Ends with stop codon?"
    int cdsCount;	"Number of CDS blocks"
    int[cdsCount] cdsStarts; "Start positions of CDS blocks"
    int[cdsCount] cdsSizes; "Sizes of CDS blocks"
    )

