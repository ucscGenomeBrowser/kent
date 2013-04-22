table addressBook
"A simple address book"
    (
    string name primary;  "Name - first or last or both, we don't care"
    string address;  "Street address"
    string city index[6];  "City"
    uint zipCode;  "A zip code is always positive, so can be unsigned"
    char[2] state index;  "Just store the abbreviation for the state"
    )

table symbolCols
"example of enum and set symbolic columns"
    (
    int id primary auto;                                          "unique id"
    enum(male, female) sex;                          "enumerated column"
    set(cProg,javaProg,pythonProg,awkProg) skills index;   "set column"
    )
