table stsMapMouse
"STS marker and its position on mouse assembly"
    (
    string chrom;	"Chromosome or 'unknown'"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of STS marker"
    uint score;	        "Score of a marker = 1000/(# of placements)"
    uint identNo;	"UCSC Id No."
    uint probeId;       "Probe Identification number of STS"
    uint markerId; 	"Marker Identification number of STS"
    )
