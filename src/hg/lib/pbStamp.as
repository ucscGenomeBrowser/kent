table pbStamp
"Info needed for a Proteom Browser Stamp"
    (
    char[40] stampName;	"Short Name of stamp"
    char[40] stampTable;"Database table name of the stamp (distribution) data"
    char[40] stampTitle;"Stamp Title to be displayed"
    int len;		"number of x-y pairs"
    float  xmin;	"x minimum"
    float  xmax;	"x maximum"
    float  ymin;	"y minimum"
    float  ymax;	"y maximum"
    string stampDesc;	"Description of the stamp"
    )
