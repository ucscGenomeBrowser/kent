table cdsPick
"Information on CDS that we picked from competing cdsEvidence"
    (
    string name;	"Name of transcript"
    int start;		"CDS start within transcript, zero based"
    int end;		"CDS end, non-inclusive"
    string source;	"Source of best evidence"
    double score;	"Higher is better. Typically from 0 to 30000"
    ubyte startComplete;"Starts with ATG?"
    ubyte endComplete;	"Ends with stop codon?"
    string swissProt;	"Matching swissProt if available."
    string uniProt;	"Matching uniProt if available."
    string refProt;	"RefSeq protein if available."
    string refSeq;	"RefSeq transcript if available."
    string genbank;	"Best scoring genbank accession."
    double wBorfScore;  "Reweighted bestorf score."
    )
