CREATE TABLE Assembly (
    id int not null,
    dbSnpBuild smallint not null,
    genomeBuild varchar(255) not null,
    groupLabel varchar(255) not null,
    current varchar(255) not null,
    SnpStat int not null,
    PRIMARY KEY(id)
);
