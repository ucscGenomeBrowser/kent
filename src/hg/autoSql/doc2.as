simple point
"A three dimensional point"
    (
    float x;  "Horizontal coordinate"
    float y;  "Vertical coordinate"
    float z;  "In/out of screen coordinate"
    )

simple color
"A red/green/blue format color"
    (
    ubyte red;  "Red value 0-255"
    ubyte green; "Green value 0-255"
    ubyte blue;  "Blue value 0-255"
    )

object face
"A face of a three dimensional solid"
    (
    simple color color;  "Color of this face"
    int pointCount;    "Number of points in this polygon"
    uint[pointCount] points;   "Indices of points that make up face in polyhedron point array"
    )

table polyhedron
"A solid three dimensional object"
    (
    int faceCount;  "Number of faces"
    object face[faceCount] faces; "List of faces"
    int pointCount; "Number of points"
    simple point[pointCount] points; "Array of points"
    )

