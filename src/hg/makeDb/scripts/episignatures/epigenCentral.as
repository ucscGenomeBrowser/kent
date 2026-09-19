table epigenCentral
"EpigenCentral DNA methylation episignature CpG probes"
(
string chrom;             "Reference sequence chromosome"
uint chromStart;          "Start position"
uint chromEnd;            "End position"
string name;              "CpG probe ID"
uint score;               "Score"
char[1] strand;           "+, -, or ."
uint thickStart;          "Start of display region"
uint thickEnd;            "End of display region"
uint reserved;            "Color assigned to the direction of the strongest episignature"

float maxAbsDelta;        "Largest absolute delta-beta|Effect size of the strongest episignature at this probe, which also sets the color"
string displaySignature;  "Strongest episignature|Gene or locus of the episignature with the largest absolute delta-beta here"
string displayDisorder;   "Disorder|Disorder of the strongest episignature"
string displayOmim;       "OMIM entry|OMIM number of that disorder"
string direction;         "Direction|Gain or loss of methylation in cases, for the strongest episignature"
uint sigCount;            "Episignatures at this probe"
lstring signatureList;    "All episignatures at this probe"
lstring comparisonTable;  "Dynamic comparison table"
)
