table tableDescriptions
"Descriptive information about database tables from autoSql / gbdDescriptions"
    (
    string  tableName;	"Name of table (with chr*_ replaced with chrN_)"
    lstring autoSqlDef;	"Contents of autoSql (.as) table definition file"
    string  gbdAnchor;	"Anchor for table description in gbdDescriptions.html"
    )
