# UniProt and Reactome Event Cross-Referenece
CREATE TABLE spReactomeEvent (
    spID varchar(40),		# UniProt protein Accession number
    eventID varchar(40),	# Reactome Event DB_ID
    eventDesc varchar(255),	# Reactome Event description
              #Indices
    KEY(spID),
    KEY(eventID)
);
