object point
"Three dimensional point"
    (
    char[12] acc;    "GenBank Accession sequence"
    int x;  "x coor"
    int y;  "y coor"
    int z;  "z coor"
    )

table polygon
"A face"
    (
    uint id;    "Unique ID"
    int pointCount;         "point count"
    object point[pointCount] points;	"Points list"
    )

table polyhedron
"A 3-d object"
    (
    uint id;   "Unique ID"
    string[2] names;   "Name of this figure"
    int polygonCount;   "Polygon count"
    object polygon[polygonCount] polygons; "Polygons"
    )
