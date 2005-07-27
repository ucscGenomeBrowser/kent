# UniProt and Reactome Protein Cross-Referenece
CREATE TABLE spReactomeId (
    spID varchar(40),		# UniProt  protein Accession number
    DB_ID varchar(40),		# Reactome DB_ID
              #Indices
    KEY(spID),
    KEY(DB_ID)
);
