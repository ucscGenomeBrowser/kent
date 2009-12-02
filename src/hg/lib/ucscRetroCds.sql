# gene symbol list of ucscRetroCds
CREATE TABLE ucscRetroCds (
    id varchar(40),	# Gene id with version
    cds varchar(512),	# cds
    KEY(id)
);
