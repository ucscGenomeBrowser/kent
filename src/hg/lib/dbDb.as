table dbDb
"Description of annotation database"
    (
    string  name;	"Short name of database.  'hg8' or the like"
    string description; "Short description - 'Aug. 8, 2001' or the like"
    lstring nibPath;	"Path to packed sequence files"
    string organism;    "Common name of organism - first letter capitalized"
    string genome;    "Unifying genome collection to which an assembly belongs"
    string defaultPos;	"Default starting position"
    int active;     "Flag indicating whether this db is in active use"
    )
