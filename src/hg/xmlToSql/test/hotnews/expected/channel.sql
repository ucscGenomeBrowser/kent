CREATE TABLE channel (
    id int not null,
    title int not null,
    link int not null,
    description int not null,
    language varchar(255) not null,
    copyright int not null,
    webMaster varchar(255) not null,
    pubDate varchar(255) not null,
    lastBuildDate varchar(255) not null,
    category varchar(255) not null,
    generator varchar(255) not null,
    docs int not null,
    PRIMARY KEY(id)
);
