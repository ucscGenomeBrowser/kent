simple point
"A two dimensional point"
    (
    int x;	"X dimension"
    int y;	"Y dimension"
    )

simple namedPoint
"A named point"
    (
    string name;	"Name of point"
    simple point point;	"X/Y coordinates"
    )

simple triangle
"A collection of three points."
    (
    string name;	"Name of triangle"
    simple point[3] points; "The three vertices"
    )

simple polygon
"A bunch of connected points."
    (
    string name;	"Name of polygon"
    int vertexCount;	"Number of vertices"
    simple point[vertexCount] vertices; "The x/y coordinates of all vertices"
    )

simple person
"Info on a person"
   (
   string firstName;	"First name"
   string lastName;	"Last name"
   bigint ssn;	"Social security number"
   )

simple couple
"Info on a group of people"
    (
    string name;	"Couple's name"
    simple person[2] members;  "Members of couple"
    )

simple group
"Info on a group of people"
    (
    string name;	"Name of group"
    int size;		"Size of group"
    simple person[size] members;	"All members of group"
    )

simple metaGroup
"Info on many groups"
    (
    string name;	"Name of group"
    int metaSize;	"Number of groups"
    simple group[metaSize] groups; "All groups in metaGroup"
    )

table metaGroupLogo
"Tie together a polygonal logo with a metaGroup"
    (
    simple polygon logo;	"Logo of meta groups"
    simple metaGroup conspiracy;	"Conspiring meta groups"
    )
