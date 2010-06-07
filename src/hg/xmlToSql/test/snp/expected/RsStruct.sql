CREATE TABLE RsStruct (
    id int not null,
    protAcc varchar(255) not null,
    protGi int not null,
    protLoc int not null,
    protResidue varchar(255) not null,
    rsResidue varchar(255) not null,
    structGi int not null,
    structLoc smallint not null,
    structResidue varchar(255) not null,
    PRIMARY KEY(id)
);
