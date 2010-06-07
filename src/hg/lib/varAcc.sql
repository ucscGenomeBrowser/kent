# varAcc table store the variant splice protein accession numbers and their parent accession numbers
CREATE TABLE varAcc (
  varAcc varchar(40) NOT NULL default '',
  parAcc varchar(40) NOT NULL default '',
  variant varchar(4) NOT NULL default '',
  KEY varAcc (varAcc),
  KEY parAcc (varAcc)
) TYPE=MyISAM;

