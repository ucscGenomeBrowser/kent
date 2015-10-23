table gtexTissueMedian
"GTEX Expression median level by tissue (RPKM levels, unmapped)"
    (
    string geneId; "Gene identifier (ensembl)"
    uint tissueCount; "Number of tissues"
    float[tissueCount] scores; "RPKM median expression levels"
    )
