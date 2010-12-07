CREATE TABLE decipherRaw (
  id     varchar(40),
  start  int(10),
  end    int(10),
  chr    varchar(40),
  mean_ratio float,
  classification_type varchar(255),
  phenotype varchar(255),
  key id (id)
) TYPE=MyISAM;

