table namedSessionDb
"Named user sessions."
    (
    string userName;	"User name (from genomewiki)."
    string sessionName;	"Name that user assigns to this session"
    lstring contents;	"CGI string of var=val&... settings."
    byte shared;	"1 if this session may be shared with other users."
    datetime firstUse;	"Session creation date."
    datetime lastUse;	"Session most-recent-usage date."
    int useCount;	"Number of times this session has been used."
    lstring settings;	".ra-formatted metadata"
    )
