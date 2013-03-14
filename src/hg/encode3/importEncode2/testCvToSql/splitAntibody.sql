#Describes where an antibody is thought to bind
CREATE TABLE cvDb_abTarget (
    term varbinary(64) not null,	# Molecular target of antibody.
    deprecated varchar(255) not null,	# If non-empty, the reason why this entry is obsolete.
    description longblob not null,	# Short description of antibody target.
    externalId varchar(255) not null,	# Identifier for target, prefixed with source of ID, usually GeneCards
    url varchar(255) not null,	# Web page associated with antibody target.
              #Indices
    PRIMARY KEY(term)
) ENGINE=INNODB;


#Information on an antibody including where to get it and what it targets
CREATE TABLE cvDb_ab (
    id int unsigned not null auto_increment,	# Unique unsigned integer identifier for this item
    term varchar(255) not null,	# A relatively short label, no more than a few words
    tag varchar(255) not null,	# A short human and machine readable symbol with just alphanumeric characters.
    deprecated varchar(255) not null,	# If non-empty, the reason why this entry is obsolete.
    description longblob not null,	# Short description of antibody itself.
    target varbinary(64) not null,	# Molecular target of antibody.
    vendorName varchar(255) not null,	# Name of vendor selling reagent.
    vendorId varchar(255) not null,	# Catalog number of other way of identifying reagent.
    orderUrl varchar(255) not null,	# Web page to order regent.
    lab varchar(255) not null,	# Scientific lab producing data.
    validation varchar(255) not null,	# How antibody was validated to be specific for target.
    label varchar(255) not null,	# A relatively short label, no more than a few words
    lots varchar(255) not null,	# The specific lots of reagent used.
              #Indices
    PRIMARY KEY(id),
    INDEX(target(16)),
    FOREIGN KEY(target) REFERENCES cvDb_abTarget(term)

) ENGINE=INNODB;

