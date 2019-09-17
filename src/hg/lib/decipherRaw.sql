CREATE TABLE decipherRaw (
  id     varchar(40),
  chr    varchar(40),
  start  int(10),
  end    int(10),
  mean_ratio varchar(255),
  genotype text,
  variant_class text,
  inheritance text,
  pathogenicity text,
  contribution text,
  phenotypes text,
  key id (id)
) ENGINE=MyISAM;

