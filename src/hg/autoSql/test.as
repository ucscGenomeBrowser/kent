simple pt
"Two dimensional point"
    (
    int x; "x coor"
    int y; "y coor"
    )

object point
"Three dimensional point"
    (
    char[12] acc;    "GenBank Accession sequence"
    int x;  "x coor"
    int y;  "y coor"
    int z;  "z coor"
    simple pt pt;  "Transformed point."
    )

table polygon
"A face"
    (
    uint id;    "Unique ID"
    int pointCount;         "point count"
    object point[pointCount] points;	"Points list"
    simple pt[pointCount] persp;  "Points after perspective transformation"
    )

table polyhedron
"A 3-d object"
    (
    uint id;   "Unique ID"
    string[2] names;   "Name of this figure"
    int polygonCount;   "Polygon count"
    object polygon[polygonCount] polygons; "Polygons"
    simple pt[2] screenBox; "Bounding box in screen coordinates"
    )

simple twoPoint
"Two points back to back"
    (
    char[12] name; "name of points"
    simple pt a;  "point a"
    simple pt b; "point b"
    simple pt[2] points; "points as array"
    )

table stringArray
"An array of strings"
    (
    short numNames;	"Number of names"
    string[numNames] names;   "Array of names"
    )
