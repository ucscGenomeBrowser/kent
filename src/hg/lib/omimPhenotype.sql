CREATE TABLE omimPhenotype (
    omimId int not null,		# OMIM ID
    description blob,			# phenotype description
    phenotypeId int,			# phenotype ID
    omimPhenoMapKey int(1),		# phenotype map key
    inhMode varchar(255),               # Inheritance mode of phenotype, can be empty
    #Indices
    KEY(omimId),
    KEY(phenotypeId)
);
