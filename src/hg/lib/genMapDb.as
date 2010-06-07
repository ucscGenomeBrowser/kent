table genMapDb
"Clones positioned on the assembly by U Penn (V. Cheung)"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    int chromStart;     "Start position in chrom - negative 1 if unpositioned"
    uint chromEnd;	"End position in chrom"
    string name;	"Name of Clone"
    uint score;	        "Score - always 1000"
    char[1] strand;	"+ or -"

    string accT7; 	"Accession number for T7 BAC end sequence"
    uint startT7;	"Start position in chrom for T7 end sequence"
    uint endT7;		"End position in chrom for T7 end sequence"
    char[1] strandT7;   "+ or -"
    string accSP6; 	"Accession number for SP6 BAC end sequence"
    uint startSP6;	"Start position in chrom for SP6 end sequence"
    uint endSP6;	"End position in chrom for SP6 end sequence"
    char[1] strandSP6;  "+ or -"
    string stsMarker; 	"Name of STS marker found in clone"
    uint stsStart;	"Start position in chrom for STS marker"
    uint stsEnd;	"End position in chrom for STS marker"
    )
