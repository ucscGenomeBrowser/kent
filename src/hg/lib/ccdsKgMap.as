table ccdsKgMap
"mapping between CCDS ids and Known Genes by similarity"
    (
    string ccdsId;        "CCDS id with version "
    string geneId;        "Id of other gene"
    string chrom;         "Chromosome or scaffold of other gene"
    uint chromStart;      "Chromosome start of other gene"
    uint chromEnd;        "Chromosome end of other gene"
    float cdsSimilarity;  "CDS similarity by genomic overlap"
    )
