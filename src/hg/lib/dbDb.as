table dbDb
"Description of annotation database"
    (
    string  name;	"Short name of database.  'hg8' or the like"
    string description; "Short description - 'Aug. 8, 2001' or the like"
    lstring nibPath;	"Path to packed sequence files"
    string organism;    "Common name of organism - first letter capitalized"
    string defaultPos;	"Default starting position"
    int orderKey;     "Int used to control display order within a genome"
    int active;     "Flag indicating whether this db is in active use"
    string genome;    "Unifying genome collection to which an assembly belongs"
    )
