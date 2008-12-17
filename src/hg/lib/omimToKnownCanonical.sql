#Link together an OMIM ID with a Known Canonical Gene ID
CREATE TABLE omimToKnownCanonical (
    omimId varchar(40) not null,# OMIM ID
    kgId varchar(40) not null,	# Canonical Known Gene ID
              #Indices
    KEY(omimId),
    KEY(kgId)
);
