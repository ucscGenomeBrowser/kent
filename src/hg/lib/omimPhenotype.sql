CREATE TABLE omimPhenotype (
    omimId int not null,		# OMIM ID
    description blob,			# disorder description
    phenotypeId int,			# phnoteype ID
    omimPhenoMapKey int(1),		# phenotype map key
    #Indices
    KEY(omimId),
    KEY(phenotypeId)
);
