table hgFindSpec
"This defines a search to be performed by hgFind."
    (
    string searchName;		"Unique name for this search.  Defaults to searchTable if not specified in .ra."
    string searchTable;		"(Non-unique!) Table to be searched.  (Like trackDb.tableName: if split, omit chr*_ prefix.)"
    string searchMethod;	"Type of search (exact, prefix, fuzzy)."
    string searchType;		"Type of search (bed, genePred, knownGene etc)."
    ubyte shortCircuit;		"If nonzero, and there is a result from this search, jump to the result instead of performing other searches."
    string termRegex;		"Regular expression (see man 7 regex) to eval on search term: if it matches, perform search query."
    string query;		"sprintf format string for SQL query on a given table and value."
    string xrefTable;		"If search is xref, perform xrefQuery on search term, then query with that result."
    string xrefQuery;		"sprintf format string for SQL query on a given (xref) table and value."
    float searchPriority;	"0-1000 - relative order/importance of this search.  0 is top."
    string searchDescription;	"Description of table/search (default: trackDb.{longLabel,tableName})"
    lstring searchSettings;	"Name/value pairs for searchType-specific stuff."
)
