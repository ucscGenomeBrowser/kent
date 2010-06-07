#Link together a Known Gene ID, Swiss-Prot/TrEMBL accession and an alias
CREATE TABLE kgSpAlias (
    kgID varchar(40) not null,	# Known Gene ID
    spID varchar(40),		# SWISS-PROT/TrEMBL protein Accession number
    alias varchar(40),		# Alias, could be either gene alias or protein alias
              #Indices
    KEY(kgID),
    KEY(spID),
    KEY(alias)
);
