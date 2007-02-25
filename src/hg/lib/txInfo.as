table txInfo
"Various bits of information about a transcript from the txGraph/txCds system (aka KG3)"
    (
    string name;	"Name of transcript"
    string sourceAcc;	"Accession of genbank transcript patterned on (may be refSeq)"
    ubyte isRefSeq;	"Is a refSeq"
    int orfSize;	"Size of ORF"
    double bestorfScore; "Score of best CDS according to bestorf"
    ubyte startComplete;  "Starts with ATG"
    ubyte endComplete; "Ends with stop codon"
    ubyte nonsenseMediatedDecay; "If true, is a nonsense mediated decay candidate."
    ubyte retainedIntronInCds; "If true has a retained intron in coding region"
    ubyte selenocysteine; "If true TGA codes for selenocysteine"
    ubyte genomicFrameShift; "True if genomic version has frame shift we cut out"
    ubyte genomicStop;    "True if genomic version has stop codon we cut out"
    )
