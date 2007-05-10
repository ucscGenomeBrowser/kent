table rnaPLFold
"RNA PL Fold -- RNA Partition Function Local Fold"
    (
    string	chrom;		"Reference sequence chromosome or scaffold"
    uint	chromStart;	"chromStart for reference marker"
    uint	chromEnd;	"chromEnd for last marker in list"
    uint	span;	 	"Number of positions covered by colorIndex values"
    string	colorIndex;	"PLFold values encoded as indexes in a color array"
    )
