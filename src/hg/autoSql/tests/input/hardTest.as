object point
"Three dimensional point"
    (
    int x;  "x coor"
    int y;  "y coor"
    int z;  "z coor"
    )

table autoTest
"Just a test table"
    (
    uint id; "Unique ID"
    char[12] shortName; "12 character or less name"
    string longName; "full name"
    string[3] aliases; "three nick-names"
    object point threeD;  "Three dimensional coordinate"
    int ptCount;  "number of points"
    short[ptCount] pts;  "point list"
    int difCount;  "number of difs"
    Ubyte [difCount] difs; "dif list"
    int[2] xy;  "2d coordinate"
    int valCount; "value count"
    string[valCount] vals; "list of values"
    double dblVal; "double value"
    float fltVal; "float value"
    double[valCount] dblArray; "double array"
    float[valCount] fltArray; "float array"
    )
