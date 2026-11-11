table myVariantsShares
"A share link giving anyone who holds it access to one My Annotations project."
    (
    uint id;           "Auto-increment primary key"
    string ownerUser;  "Login of the project's owner"
    string shareToken; "48-char URL-safe token; holding it is what grants access"
    uint projectId;    "myVariantsProjects.id of the shared project"
    ubyte permission;  "0=view, 1=edit"
    string createdAt;  "Timestamp of creation"
    )
