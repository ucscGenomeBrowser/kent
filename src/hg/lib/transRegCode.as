table transRegCode
"Transcription factor binding sites from ChIP-chip experiments and conservation"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint chromStart;    "Start position in chromosome"
    uint chromEnd;      "End position in chromosome"
    string name;        "Name of transcription factor"
    uint score;         "Score from 0 to 1000"
    string chipEvidence; "Evidence strength from ChIP-chip assay"
    uint consSpecies;	"Number of species conserved in"
    )
