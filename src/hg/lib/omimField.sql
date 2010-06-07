CREATE TABLE omimField (
  omimId    varchar(40) NOT NULL, # OMIM ID
  fieldType char(2) NOT NULL,	  # Field type
  startPos  int(12) NOT NULL,	  # starting offset position
  length    int(12) NOT NULL,	  # length of the text of this field
  KEY omimId (omimId(10)),		
  KEY totalID (omimId(10), fieldType)
) TYPE=MyISAM;

