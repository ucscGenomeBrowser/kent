CREATE TABLE featureSynonym (
    id int not null,
    isInternal tinyint not null,
    synonymId int not null,
    pubId varchar(255) not null,
    isCurrent tinyint not null,
    PRIMARY KEY(id)
);
