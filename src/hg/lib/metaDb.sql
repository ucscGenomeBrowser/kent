#This contains metadata for a table, file or other predeclared object type.
CREATE TABLE metaDb (
    obj varchar(255) not null,	        # Object name or ID.
    var varchar(255) not null,	        # Metadata variable name.
    val varchar(2048) not null,	        # Metadata value.
              #Indices
    PRIMARY KEY(obj,var),
    INDEX varKey (var,val(64))
);
