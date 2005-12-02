CREATE TABLE ExchangeSet (
    xmlnsXsi varchar(255) not null,
    xmlns varchar(255) not null,
    xsiSchemaLocation longtext not null,
    specVersion float not null,
    dbSnpBuild smallint not null,
    generated varchar(255) not null,
    PRIMARY KEY(dbSnpBuild)
);
