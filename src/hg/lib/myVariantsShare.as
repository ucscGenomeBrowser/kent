table myVariantsShares
"A share record linking an owner's myVariants project to a recipient."
    (
    uint id;              "Auto-increment primary key"
    string ownerUser;     "Owner who created the share"
    string shareToken;    "48-char URL-safe token"
    string project;       "Project name, or * for all"
    string db;            "Assembly (hg38, mm39, etc.)"
    ubyte permission;     "0=read-only, 1=read-write"
    string targetUser;    "Specific user, or NULL for anyone with link"
    string label;         "Optional human-readable label"
    string createdAt;     "Timestamp of creation"
    )
