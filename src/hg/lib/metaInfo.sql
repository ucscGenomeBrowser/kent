#lastAccessed statistics for customTrash DB tables
CREATE TABLE metaInfo (
    name varchar(255) not null,	# customTrash table name
    useCount int not null,	# Number of times this table used
    lastUse datetime not null,	# table most-recent-usage date.
              #Indices
    PRIMARY KEY(name)
);
