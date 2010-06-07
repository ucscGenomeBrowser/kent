# gene symbol list of ucscRetroCds
CREATE TABLE ucscRetroCds (
    id varchar(40),	# Gene id with version
    cds longtext,	# cds
    KEY(id)
);
