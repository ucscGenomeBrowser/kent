CREATE TABLE featureloc (
    id int not null,
    srcfeatureId varchar(255) not null,
    fmin int not null,
    fmax int not null,
    strand smallint not null,
    isFminPartial tinyint not null,
    isFmaxPartial tinyint not null,
    locgroup tinyint not null,
    rank tinyint not null,
    residueInfo int not null,
    PRIMARY KEY(id)
);
