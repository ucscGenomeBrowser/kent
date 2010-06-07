table knownInfo
"Auxiliary info about a known gene"
    (
    string name;     "connects with genieKnown->name"
    string transId;  "Transcript id. Genie generated ID."
    string geneId;   "Gene (not trascript) id"
    uint geneName;   "Connect to geneName table"
    uint productName;  "Connects to productName table"
    string proteinId;  "Genbank accession of protein?"
    string ngi;        "Genbank gi of nucleotide seq."
    string pgi;        "Genbank gi of protein seq."
    )
