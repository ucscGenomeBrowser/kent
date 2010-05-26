#Text to Genome project article data table
CREATE TABLE t2gArticle (
    pmcId bigint not null,	# PubMedCentral ID
    displayId varchar(255) not null,	# display ID shown on browser
    authors varchar(6000) default null,	# author list for this article
    title varchar(2000) default null,	# article title
    journal varchar(1000) default null,	# source journal
    year varchar(255) default null,	# publication year
    pages varchar(255) default null,	# publication pages
    pmid varchar(255) default null,	# Pubmed ID
    abstract blob not null,	# article abstract
              #Indices
    PRIMARY KEY(pmcId),
    KEY displayIdx(displayId)
);
