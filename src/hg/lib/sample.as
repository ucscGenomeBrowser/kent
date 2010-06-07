table sample
"Any track that has samples to display as y-values (first 6 fields are bed6)"
(
string chrom;         "Reference sequence chromosome or scaffold"
uint chromStart;      "Start position in chromosome"
uint chromEnd;        "End position in chromosome"
string name;          "Name of item"
uint   score;         "Score from 0-1000"
char[2] strand;        "# + or -"

uint sampleCount;                   "number of samples total"
uint[sampleCount] samplePosition;   "bases relative to chromStart (x-values)"
int[sampleCount] sampleHeight;     "the height each pixel is drawn to [0,1000]"
)

