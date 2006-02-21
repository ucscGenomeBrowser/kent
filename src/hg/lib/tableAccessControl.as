table tableAccessControl
"List of tables which may be displayed only on certain virtual hosts."
    (
    string name;      "Track/table name (may omit chr*_ if split)."
    string host;      "Virtual host name on which table may be displayed."
    )
