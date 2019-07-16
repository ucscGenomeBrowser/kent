#This table contains the same data as the genemap2.txt file downloaded from OMIM
CREATE TABLE omimGeneMap2 (
    location varchar(255),	# Cyto Location
    compCtyoLoc varchar(255),	# Computed Cyto Location
    omimId int not null,	# Mim Number
    geneSymbol varchar(255),	# Gene Symbol(s)
    geneName longblob,	# Gene Name
    approvedSymbol varchar(255),	# Approved Symbol
    entrez varchar(255),	# Entrez Gene ID
    ensGeneId varchar(255),	# Ensembl Gene ID
    comments longblob,	# Comments
    phenotypes longblob,	# Phenotypes
    mmGeneId longblob,	# Mouse Gene Symbol/ID
              #Indices
    KEY(omimId)
);
