CREATE TABLE featureRelationship (
    id int not null,
    typeId int not null,
    subjectId int not null,
    rank tinyint not null,
    PRIMARY KEY(id)
);
