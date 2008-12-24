table addressBook
"A simple address book"
    (
    string name;  "Name - first or last or both, we don't care"
    string address;  "Street address"
    string city;  "City"
    uint zipCode;  "A zip code is always positive, so can be unsigned"
    char[2] state;  "Just store the abbreviation for the state"
    )

table symbolCols
"example of enum and set symbolic columns"
    (
    int id;                                          "unique id"
    enum(male, female) sex;                          "enumerated column"
    set(cProg,javaProg,pythonProg,awkProg) skills;   "set column"
    )
