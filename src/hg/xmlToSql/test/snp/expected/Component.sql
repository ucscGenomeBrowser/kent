CREATE TABLE Component (
    id int not null,
    componentType varchar(255) not null,
    ctgId bigint not null,
    accession varchar(255) not null,
    name varchar(255) not null,
    chromosome tinyint not null,
    start int not null,
    end int not null,
    orientation varchar(255) not null,
    gi int not null,
    groupTerm varchar(255) not null,
    contigLabel varchar(255) not null,
    PRIMARY KEY(id)
);
