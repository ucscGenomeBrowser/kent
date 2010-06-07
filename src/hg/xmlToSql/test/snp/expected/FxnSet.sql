CREATE TABLE FxnSet (
    id int not null,
    geneId int not null,
    symbol varchar(255) not null,
    fxnClass varchar(255) not null,
    mrnaAcc varchar(255) not null,
    mrnaVer tinyint not null,
    protAcc varchar(255) not null,
    protVer tinyint not null,
    readingFrame tinyint not null,
    allele varchar(255) not null,
    residue varchar(255) not null,
    aaPosition int not null,
    PRIMARY KEY(id)
);
