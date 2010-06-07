table columnInfo
"Meta data information about a particular column in the database."
(
string name; 	"Column name"
string type;    "Column type, i.e. int, blob, varchar."
string nullAllowed; "Are NULL values allowed?"
string key;     "Is this field indexed? If so how."
string defaultVal; "Default value filled."
string extra;   "Any extra information."
)
