CREATE TABLE Rs (
    rsId int not null,
    snpClass varchar(255) not null,
    snpType varchar(255) not null,
    molType varchar(255) not null,
    genotype varchar(255) not null,
    Validation int not null,
    Create int not null,
    Update int not null,
    Sequence int not null,
    Assembly int not null,
    Het int not null,
    PRIMARY KEY(rsId)
);
