
CREATE TABLE affyHuEx1Annot (
  numIndependentProbes smallint not null,
  probesetId int(11) not null,
  exonClustId int(11) not null,
  numNonOverlapProbes smallint not null,
  probeCount smallint not null,
  transcriptClustId int(11) not null,
  probesetType smallint not null,
  numXHybeProbe smallint not null,
  psrId int(11) not null,
  level varchar(10) not null,
  evidence varchar(255) not null,
  bounded smallint not null,
  cds smallint not null,
  PRIMARY KEY (probesetId)
);
  

