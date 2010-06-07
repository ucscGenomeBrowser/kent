CREATE TABLE SnpStat (
    id int not null,
    mapWeight varchar(255) not null,
    chromCount tinyint not null,
    placedContigCount tinyint not null,
    unplacedContigCount tinyint not null,
    seqlocCount tinyint not null,
    hapCount tinyint not null,
    PRIMARY KEY(id)
);
