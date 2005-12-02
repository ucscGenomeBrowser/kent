CREATE TABLE PrimarySequence (
    id int not null,
    dbSnpBuild smallint not null,
    gi int not null,
    source varchar(255) not null,
    PRIMARY KEY(id)
);
