#publications track article metadata table
CREATE TABLE pubsArticle (
    articleId bigint not null,	# internal article ID, created during download
    extId varchar(255) not null,	# publisher ID e.g. PMCxxxx or doi or sciencedirect ID
    pmid bigint not null,               # PubmedID if available
    doi varchar(255) not null,               # DOI if available
    source varchar(255) not null,       # data source, e.g. elsevier or pmcftp
    citation varchar(2000) default null,	# source journal citation
    year int not null,	# year of publication or 0 if not defined
    title varchar(6000) default null,	# article title
    authors varchar(12000) default null,	# author list for this article
    firstAuthor varchar(255) default null,	# first author family name
    abstract varchar(32000) not null,	# article abstract
    url varchar(1000) default null,	# url to fulltext of article
    dbs varchar(500) default null,      # list of DBs with matches to this article
              #Indices
    PRIMARY KEY(articleId),
    KEY extIdx(extId),
    FULLTEXT INDEX (citation, title, authors, abstract)
);
