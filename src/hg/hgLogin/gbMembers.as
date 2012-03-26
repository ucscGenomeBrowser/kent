table gbMembers
"UCSC Genome Browser members"
    (
    uint idx;          "auto-increment unique ID"
    string  userName;  "Name used to login"
    string  realName;  "Full name"
    string  password;  "Encrypted password"
    string  email;     "Email address"
    string  lastUse;   "Last date the user log in"
    char[1] activated; "Account activated? Y or N"
    string  dateAuthenticated; "Date the account activated via email"
    )

