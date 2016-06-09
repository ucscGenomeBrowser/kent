table gbMemberToken
"Maps random token to values of gbMembers.userName"
    (
    uint token;         "Random nonzero number"
    string userName;    "Name used to login in hgLogin"
    string createTime;  "Date and time at which token was created"
    string validUntil;  "Date and time until which token will be removed from table, NULL: do not remove"
    )
