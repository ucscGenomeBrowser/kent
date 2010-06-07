object point
"A simple point"
    (
    int x;  "horizontal position left to right"
    int y;  "vertical position top to bottom"
    )

table autoTest
"Just a test"
    (
    uint id; "Unique ID"
    char[12] shortName; "12 character or less name"
    string longName; "full name"
    string[3] aliases; "three nick-names"
    int ptCount;  "number of points"
    short[ptCount] pts;  "point list"
    int difCount;  "number of difs"
    Ubyte [difCount] difs; "dif list"
    object point xy;  "2d coordinate"
    int valCount; "value count"
    string[valCount] vals; "list of values"
    )
