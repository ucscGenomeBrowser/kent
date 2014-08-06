table gtexData
"GTEX Expression data (RPKM levels, unmapped)"
    (
    string geneId; "Gene identifier (ensembl)"
    uint sampleCount; "Number of samples"
    float[sampleCount] sampleLevels; "RPKM expression levels"
    )
