# UCSC offset to Ensembl coordinates
CREATE TABLE ensemblLift (
    chrom varchar(255) not null,      # Ensembl chromosome name
    offset int unsigned not null,     # offset to add to UCSC position 
              #Indices
    PRIMARY KEY(chrom(15))
);
