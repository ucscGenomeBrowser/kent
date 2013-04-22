
table nonGenomic
"A table with non-genomic data"
    (
    uint key;   "Private key"
    string name;  "A name in any format"
    lstring address; "Address in free format"
    )

table alreadyBinned
"A table that already has chrom and bin"
    (
    uint bin;   "Browser speedup thing"
    string chrom;       "CHromosome"
    int start;  "Start 0-based"
    int end;    "End - non-inclusive"
    string name;        "What we are normally called"
    uint id;    "What they use to look us up in database"
    )

table needBin
"A table that already chrom but needs bin"
    (
    string chrom;       "Chromosome"
    int start;  "Start 0-based"
    int end;    "End - non-inclusive"
    string name;        "What we are normally called"
    uint id;    "What they use to look us up in database"
    )

