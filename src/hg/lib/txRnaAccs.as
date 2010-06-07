table txRnaAccs
"A list of Genbank accessions associated with a transcript"
    (
    string name;	"Transcript name"
    string primary;	"Primary source of info - contains all exons"
    int accCount;	"Count of total accessions"
    string[accCount] accs; "Array of accessions.  Not all of these have all exons"
    )
