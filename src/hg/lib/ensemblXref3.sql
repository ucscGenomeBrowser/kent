# this table holds cross-reference data betwee
# Ensembl gene, transcript, protein, TrEMBL accession, 
# SWISS-PROT dipslay ID, and SWISS-PROT accession
CREATE TABLE ensemblXref3 (
  gene char(20) NOT NULL default '',
  geneVer char(4) NOT NULL default '',
  transcript char(20) NOT NULL default '',
  transcriptVer char(4) NOT NULL default '',
  protein char(20) NOT NULL default '',
  proteinVer char(4) NOT NULL default '',
  tremblAcc char(20),
  swissDisplayId char(20),
  swissAcc char(20),
  KEY protein (protein),
  KEY transcript (transcript),
  KEY gene (gene),
  KEY tremblAcc (tremblAcc),
  KEY siwssDisplayId (swissDisplayId),
  KEY swissAcc (swissAcc)
) TYPE=MyISAM;

