table cnpSharp 
"CNP data from Sharp lab"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint chromStart;	"Start position in chrom"
    uint chromEnd;	"End position in chrom"
    string name; 	"BAC name"
    string variationType; "Gain, Loss, Gain and Loss"
    string cytoName;	"Cytoband"
    string cytoStrain;	"gneg, gpos, etc."
    float dupPercent;	"0 to 1"
    float repeatsPercent;	"0 to 1"
    float LINEpercent;	"0 to 1"
    float SINEpercent;	"0 to 1"
    float LTRpercent;	"0 to 1"
    float DNApercent;	"0 to 1"
    float diseaseSpotsPercent;	"0 to 1"
    )
