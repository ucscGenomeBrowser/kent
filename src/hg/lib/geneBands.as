table geneBands
"Band locations of known genes"
    (
    string name; "Name of gene - hugo if possible"
    string mrnaAcc; "RefSeq mrna accession"
    int count; "Number of times this maps to the genome"
    string[count] bands; "List of bands this maps to"
    )
