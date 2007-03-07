#ensembl Genes track additional information
CREATE TABLE ensInfo (
    transcriptId varchar(255) not null,	# Ensembl transcript ID
    otterId varchar(255) not null,	# otter (Ensembl db) transcript ID
    geneId varchar(255) not null,	# Ensembl gene ID
    class varchar(255) not null,	# Ensembl gene class
    geneDesc varchar(255) not null,	# Ensembl gene description
    confidence varchar(255) not null,	# Ensembl gene confidence
              #Indices
    PRIMARY KEY(transcriptId)
);
