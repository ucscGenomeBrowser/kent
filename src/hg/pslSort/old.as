
table psc
"mRNA alignment basic info"
    (
    uint score;  "Matches minus mismatches"
    char[12] qAcc;  "mRNA accession"
    uint qStart; "Start of alignment in mRNA"
    uint qEnd; "End of alignment in mRNA"
    uint qSize; "Total mRNA size"
    char[12] tAcc; "Genomic accession"
    uint tStart; "Start of alignment in genomic"
    uint tEnd; "End of alignment in genomic"
    uint tSize; "Total genomic size"
    string blockSizes; "Size of each block."
    string qBlockStarts; "Start of each block in mRNA"
    string tBlockStarts; "Start of each block in genomic"
    )
    
