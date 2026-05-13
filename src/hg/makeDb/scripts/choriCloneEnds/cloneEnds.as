table choriCloneEnds
"CHORI zebrafish BAC library unique-concordant clone placements (NCBI Clone DB)"
(
string  chrom;            "Reference sequence chromosome or scaffold"
uint    chromStart;        "Start position (0-based)"
uint    chromEnd;          "End position"
string  name;              "Clone name (e.g. CH1073-100A1)"
uint    score;             "Always 0 (not provided in source)"
char[1] strand;            "Always + (not meaningful in source)"
uint    insertSize;        "End - start of placement, bp"
string  concordant;        "TRUE if this placement is concordant with the reference"
string  unique;            "TRUE if this placement is unique in the genome"
string  assmUnit;          "NCBI assembly unit (Primary Assembly, ALT_DRER_TU_1, etc.)"
string  oversize;          "TRUE if insertSize > 500000|Flag for likely-spurious mappings"
string  ncbiPlacementId;   "NCBI placement ID (GFF column-9 ID)"
string  placementMethod;   "NCBI placement method (end-seq)"
)
