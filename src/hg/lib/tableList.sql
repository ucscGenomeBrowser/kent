# This table is used to create a cache for the "show tables" and "describe table" commands.
# The table structure is the table name plus all fields that are returned by the describe
# table command. As the field names are as similar as possbible to MySQL, they do not follow kent src style.

CREATE TABLE tableList (
    tableName varchar(255) not null,  # table name, as in 'show tables' command
    field varchar(255),  # field name, as in 'describe table' command
    type varchar(255),   # type of field, as in 'describe table' command
    nullAllowed varchar(255),   # null allowed field of describe table command
    isKey varchar(255),  # is key field of describe table command 
    hasDefault varchar(255) NULL,  # has default field of describe table command
    extra varchar(255), # extra field of describe table command
    INDEX tableIdx(tableName) # need an index, as this table can get quite big for human
)
