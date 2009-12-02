# gene symbol list of ucscRetroCds
CREATE TABLE ucscRetroCds (
    id varchar(40),	# Gene id with version
    cds varchar(255),	# cds
    KEY(id)
);
