table ccdsGeneMap
"mapping between CCDS ids and MGC genes by similarity"
    (
    string ccdsId;        "CCDS id with version "
    string geneId;        "Id of other gene"
    string chrom;         "chromosome of other gene"
    string chromStart;    "chromosome start of other gene"
    string chromEnd;      "chromosome end of other gene"
    float cdsSimilarity;  "CDS similarity by genomic overlap"
    )
