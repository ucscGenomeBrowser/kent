CREATE TABLE decipherRaw (
  id     varchar(40),
  chr    varchar(40),
  start  int(10),
  end    int(10),
  refAllele text,
  altAllele text,
  transcript text,
  gene text,
  genotype text,
  inheritance text,
  pathogenicity text,
  contribution text,
  phenotypes text,
  key id (id)
) ENGINE=MyISAM;

