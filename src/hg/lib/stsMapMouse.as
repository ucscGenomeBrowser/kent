table stsMapMouse
"STS marker and its position on mouse assembly"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of STS marker"
    uint score;         "Score of a marker = 1000 when placed uniquely, else 1500/(#placements) when placed in multiple locations"

    uint identNo;	"UCSC Id No."
    uint probeId;       "Probe Identification number of STS"
    uint markerId; 	"Marker Identification number of STS"
    )
