CREATE TABLE Ss (
    ssId int not null,
    handle varchar(255) not null,
    batchId int not null,
    locSnpId varchar(255) not null,
    subSnpClass varchar(255) not null,
    orient varchar(255) not null,
    strand varchar(255) not null,
    molType varchar(255) not null,
    buildId smallint not null,
    methodClass varchar(255) not null,
    linkoutUrl longtext not null,
    validated varchar(255) not null,
    Sequence int not null,
    PRIMARY KEY(ssId)
);
