CREATE TABLE BaseURL (
    text longtext not null,
    urlId tinyint not null,
    resourceName varchar(255) not null,
    resourceId varchar(255) not null,
    PRIMARY KEY(urlId)
);
