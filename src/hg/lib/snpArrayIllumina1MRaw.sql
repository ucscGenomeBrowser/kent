CREATE TABLE snpArrayIllumina1MRaw (
  SNPname varchar(20),
  dbSNPrefStrand char(1),
  ILMNrefStrand char(1),
  SNPalleles varchar(20),
  GenomeBuild varchar(10),
  Chromosome varchar(10),
  Coordinate int,
  KEY SNPname (SNPname)
) TYPE=MyISAM;

