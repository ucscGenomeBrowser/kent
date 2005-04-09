table ccdsGeneMap
"mapping between CCDS ids and other gene tables by similarity"
    (
    string ccdsId;        "CCDS id with version "
    string geneId;        "Id of other gene"
    string chrom;         "chromosome of other gene"
    uint chromStart;      "chromosome start of other gene"
    uint chromEnd;        "chromosome end of other gene"
    float cdsSimilarity;  "CDS similarity by genomic overlap"
    )
