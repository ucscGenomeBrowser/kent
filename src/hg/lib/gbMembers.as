table gbMembers
"UCSC Genome Browser members"
    (
    uint idx;          "auto-increment unique ID"
    string userName;  "Name used to login"
    string realName;  "Full name"
    string password;  "Encrypted password"
    string email;     "Email address"
    string lastUse;   "Last date the user log in/log out/change password"
    string newPassword; "Password generated for the mail-a-new-password feature"
    string newPasswordExpire; "Expiration date of the new password generated"
    string dateActivated; "Date the account activated via email"
    string emailToken; "Security token used in the email to the user"
    string emailTokenExpires; "Expiration date of the emailToken"
    char [1] passwordChangeRequired; "Password change required?"
    char[1] accountActivated; "Account activated? Y or N"
    )
