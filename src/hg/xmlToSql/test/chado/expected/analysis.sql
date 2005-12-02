CREATE TABLE analysis (
    id int not null,
    sourcename varchar(255) not null,
    timeexecuted varchar(255) not null,
    sourceversion float not null,
    program varchar(255) not null,
    programversion float not null,
    PRIMARY KEY(id)
);
