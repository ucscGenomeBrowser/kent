table gbStatus
"Parallel to gbCdnaInfo to target state status an entry"
    (
    string acc; "Genbank/EMBL accession (without .version)"
    ushort version; "Version number in Genbank"
    string moddate; "Date last modified, in SQL DATE format/ascii YYYY-MM-DD"
    enum('EST','mRNA') type; "Either EST or mRNA.
    enum('GenBank','RefSeq') srcDb; "NCBI database"
    enum('native','xeno') orgCat; "category of entry organism relative to this database"
    uint gbSeq; "id in gbSeq table"
    uint numAligns; "number of alignments"
    char(8) seqRelease; "genbank release where sequence was last update"
    char(10) seqUpdate; "genbank update where sequence was last update"
    char(8) metaRelease; "genbank release where metadata was last update"
    char(10) metaUpdate; "genbank update where metadata was last update"
    char(8) extRelease; "genbank release where gbExtFile was last update"
    char(10) extUpdate; "genbank update where gbExtFile was last update"
    string timestamp; "time of last update of this entry in genome browser DB"
    )
