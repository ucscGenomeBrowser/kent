table dbDb
"Description of annotation database"
    (
    string  name;	"Short name of database.  'hg8' or the like"
    string description; "Short description - 'Aug. 8, 2001' or the like"
    lstring nibPath;	"Path to packed sequence files"
    string organism;    "Common name of organism - first letter capitalized"
    string defaultPos;	"Default starting position"
    int active;         "Flag indicating whether this db is in active use"
    int orderKey;       "Int used to control display order within a genome"
    string genome;      "Unifying genome collection to which an assembly belongs"
    string scientificName;  "Genus and species of the organism; e.g. Homo sapiens"
    string htmlPath;    "path in /gbdb for assembly description"
    byte hgNearOk;      "Have hgNear for this?"
    byte hgPbOk;        "Have pbTracks for this?"
    string sourceName;  "Source build/release/version of the assembly"
    int taxId;          "NCBI Taxonomy ID for genome"
    )
