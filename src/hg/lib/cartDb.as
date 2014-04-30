table cartDb
"A simple id/contents pair for persistent storing of cart variables"
    (
    uint id;	"Cart ID"
    lstring contents; "Contents - encoded variables"
    int reserved;     "Reserved field, currently always zero"
    string firstUse; "First time this was used"
    string lastUse; "Last time this was used"
    int useCount; "Number of times used"
    string sessionKey; "Random key to protect session ids"
    )
