table userDb
"User Database"
    (
    uint id;              "auto-increment item ID"
    lstring contents;     "encoded variables */
    int reserved;         "currently always zero"
    string firstUse;      "first time this was used"
    string lastUse;       "last time this was used"
    uint useCount;        "number of times used"
    string sessionKey;    "random key for session security"
    )
