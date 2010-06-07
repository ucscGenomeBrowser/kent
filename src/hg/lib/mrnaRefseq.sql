CREATE TABLE mrnaRefseq (
  mrna varchar(40) NOT NULL default '',
  refseq varchar(40) NOT NULL default '',
  KEY mrna (mrna),
  KEY refseq (refseq)
) TYPE=MyISAM;

