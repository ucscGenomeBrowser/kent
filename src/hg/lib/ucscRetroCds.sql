# gene symbol list of ucscRetroCds
CREATE TABLE ucscRetroCds (
    id varchar(40),	# Gene id with version
    cds varchar(40),	# cds
    KEY(id)
);
