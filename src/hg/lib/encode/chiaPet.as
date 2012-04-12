table chiaPet
"ChIA-pet data"
(
string chrom;      "Reference sequence chromosome or scaffold"
uint   chromStart; "Start position in chromosome"
uint   chromEnd;   "End position in chromosome"
string name;       "Name of item"
uint score;        "Darkness. Recommended to do # of PETS times 200 with a max of 1000 at typical coverage levels"
char[1] strand;  "+ or - or . if unknown"
uint clusterId; "Joins together with other regions in interaction pair or larger cluster"
double mappingScore; "Log-odds score of mapping quality.  0.0 for unknown"
)
