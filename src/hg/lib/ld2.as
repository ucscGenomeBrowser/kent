table ld2
"Linkage Disequilibrium values computed by Haploview and encoded by UCSC."
    (
    string	chrom;		"Reference sequence chromosome or scaffold"
    uint	chromStart;	"chromStart for reference marker"
    uint	chromEnd;	"chromEnd for last marker in list"
    string	name;		"rsId at chromStart"
    uint	ldCount; 	"Number of markers with LD values"
    lstring	dprime;		"Encoded list (length=ldCount) of D' values"
    lstring	rsquared;	"Encoded list (length=ldCount) of r^2 values"
    lstring	lod;		"Encoded list (length=ldCount) of LOD values"
    char	avgDprime;	"Encoded average D' magnitude for dense display"
    char	avgRsquared;	"Encoded average r^2 value for dense display"
    char	avgLod;		"Encoded average LOD value for dense display"
    char	tInt;		"Encoded T-int value between this and next marker"
    )
