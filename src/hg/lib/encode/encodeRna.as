TABLE encodeRna
"Describes RNAs in the encode regions"
(
    string   chrom;		"Reference sequence chromosome or scaffold"
    uint     chromStart;	"Start position in chromosome"
    uint     chromEnd;		"End position in chromosome"
    string   name;		"Name of gene"
    uint     score;		"Score from 0 to 1000"
    string   strand;	        "Strand + or -"
    string   source;		"Source as in Sean Eddy's files."
    string   type;		"Type - snRNA, rRNA, tRNA, etc."
    float    fullScore;		"Score as in Sean Eddys files."
    uint     isPsuedo;		"TRUE(1) if psuedo, FALSE(0) otherwise"
    uint     isRmasked;		"TRUE(1) if >10% is RepeatMasked, FALSE(0) otherwise"
    uint     isTranscribed;	"TRUE(1) if >10% falls within a transfrag or TAR, FALSE(0) otherwise"
    uint     isPrediction;	"TRUE(1) if an evoFold prediction, FALSE(0) otherwise"
    string   transcribedIn;	"List of experiments transcribed in"
)
