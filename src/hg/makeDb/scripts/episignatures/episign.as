table episign
"EpiSign episignature loci and imprinting or promoter regions"
(
string chrom;             "Reference sequence chromosome"
uint chromStart;          "Start position"
uint chromEnd;            "End position"
string name;              "Disorder code|EpiSign abbreviation of the disorder"
uint score;               "Score"
char[1] strand;           "+, -, or ."
uint thickStart;          "Start of display region"
uint thickEnd;            "End of display region"
uint reserved;            "Color: episignature or DMR"

string gene;              "Gene or region|Causative gene, or the DMR"
lstring disorders;        "Disorders"
lstring signatures;       "Episignatures|EpiSign IDs of the signatures"
uint sigCount;            "Number of episignatures"
string type;              "Type|Episignature or imprinting/promoter DMR"
string dmrId;             "DMR ID|EpiSign name of the region"
string label;             "Label|Disorder code and gene, shown on the track"
)
