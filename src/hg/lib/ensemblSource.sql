#Map Ensembl gene transcript ID to source category
CREATE TABLE ensemblSource (
    name varchar(255) not null,	# Same as name field in ensGene
    source varchar(255) not null,	# source category
              #Indices
    PRIMARY KEY(name(15))
);
