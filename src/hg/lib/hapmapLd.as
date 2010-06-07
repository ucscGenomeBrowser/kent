table hapmapLd
"Linkage Disequilibrium values from the HapMap project"
    (
    string	chrom;		"Reference sequence chromosome or scaffold"
    uint	chromStart;	"chromStart for reference marker"
    uint	chromEnd;	"chromEnd for last marker in list"
    string	name;		"rsId at chromStart"
    uint	score;	 	"Number of markers with LD values"
    string	dprime;		"Encoded lists of D' values"
    string	rsquared;	"Encoded lists of r^2 values"
    string	lod;		"Encoded lists of LOD values"
    )
