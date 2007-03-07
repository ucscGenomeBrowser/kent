#ensembl Genes track additional information
CREATE TABLE ensInfo (
    name varchar(255) not null,	# Ensembl transcript ID
    otherId varchar(255) not null,	# other ID
    geneId varchar(255) not null,	# Ensembl gene ID
    class varchar(255) not null,	# Ensembl gene class
    geneDesc varchar(512) not null,	# Ensembl gene description
    confidence varchar(255) not null,	# Ensembl gene confidence
              #Indices
    PRIMARY KEY(name)
);
