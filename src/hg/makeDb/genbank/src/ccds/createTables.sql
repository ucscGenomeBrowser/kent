DROP TABLE IF EXISTS AccessionRejectionCriteria;
CREATE TABLE AccessionRejectionCriteria (
	acc_rejection_uid int PRIMARY KEY  NOT NULL ,
	name varchar (64) NOT NULL ,
	description text NULL 
) 
;

DROP TABLE IF EXISTS Accessions;
CREATE TABLE Accessions (
	accession_uid int PRIMARY KEY  NOT NULL ,
	nuc_acc varchar (64) NOT NULL ,
	nuc_version int NULL ,
	nuc_gi int NULL ,
	prot_acc varchar (64) NOT NULL ,
	prot_version int NULL ,
	prot_gi int NULL ,
	organization_uid int NOT NULL ,
	alive tinyint(1) NOT NULL 
) 
;

DROP TABLE IF EXISTS Accessions_GroupVersions;
CREATE TABLE Accessions_GroupVersions (
	accession_uid int NOT NULL ,
	group_version_uid int NOT NULL ,
	ccds_status_val_uid int NOT NULL ,
	original_member tinyint(1) NOT NULL ,
	was_public tinyint(1) NOT NULL 
) 
;

DROP TABLE IF EXISTS CcdsStatusVals;
CREATE TABLE CcdsStatusVals (
	ccds_status_val_uid int PRIMARY KEY  NOT NULL ,
	ccds_status varchar (50) NOT NULL 
) 
;

DROP TABLE IF EXISTS CcdsUids;
CREATE TABLE CcdsUids (
	ccds_uid int NOT NULL ,
	group_uid int NOT NULL ,
	latest_version int NOT NULL 
) 
;

DROP TABLE IF EXISTS ChromosomeAccessions;
CREATE TABLE ChromosomeAccessions (
	chromosome_accession_uid int PRIMARY KEY  NOT NULL ,
	organization_uid int NOT NULL ,
	acc varchar (64) NOT NULL ,
	version int NULL ,
	chromosome varchar (8) NOT NULL 
) 
;

DROP TABLE IF EXISTS GroupVersions;
CREATE TABLE GroupVersions (
	group_version_uid int PRIMARY KEY  NOT NULL ,
	group_uid int NOT NULL ,
	version int NOT NULL ,
	ncbi_build_number int NOT NULL ,
	first_ncbi_build_version int NOT NULL ,
	last_ncbi_build_version int NOT NULL ,
	gene_id int NOT NULL ,
	location_count int NOT NULL ,
	ccds_status_val_uid int NOT NULL ,
	ccds_version int NULL ,
	was_public tinyint(1) NOT NULL 
) 
;

DROP TABLE IF EXISTS GroupVersions_ChromosomeAccessions;
CREATE TABLE GroupVersions_ChromosomeAccessions (
	group_version_uid int NOT NULL ,
	chromosome_accession_uid int NOT NULL 
) 
;

DROP TABLE IF EXISTS Groups;
CREATE TABLE Groups (
	group_uid int NOT NULL ,
	current_version int NOT NULL ,
	tax_id int NOT NULL ,
	chromosome varchar (8) NOT NULL ,
	orientation char (1) NOT NULL 
) 
;

DROP TABLE IF EXISTS InterpretationSubtypes;
CREATE TABLE InterpretationSubtypes (
	interpretation_subtype_uid int PRIMARY KEY  NOT NULL ,
	interpretation_type_uid int NOT NULL ,
	interpretation_subtype varchar (128) NOT NULL ,
        for_public tinyint(1) NOT NULL ,
        can_edit_comment tinyint(1) NOT NULL 
) 
;

DROP TABLE IF EXISTS InterpretationTypes;
CREATE TABLE InterpretationTypes (
	interpretation_type_uid int PRIMARY KEY  NOT NULL ,
	interpretation_type varchar (128) NOT NULL ,
        for_public tinyint(1) NOT NULL
) 
;

DROP TABLE IF EXISTS Interpretations;
CREATE TABLE Interpretations (
	interpretation_uid int PRIMARY KEY  NOT NULL ,
	ccds_uid int NULL ,
	group_uid int NULL ,
	group_version_uid int NULL ,
	accession_uid int NULL ,
	parent_interpretation_uid int NULL ,
	date_time datetime NOT NULL ,
	comment text NULL ,
	val_description varchar (50) NULL ,
	char_val text NULL ,
	integer_val int NULL ,
	float_val float NULL ,
	interpretation_type_uid int NOT NULL ,
	interpretation_subtype_uid int NULL ,
	acc_rejection_uid int NULL ,
	interpreter_uid int NOT NULL ,
	program_uid int NULL ,
	reftrack_uid int NULL 
) 
;

DROP TABLE IF EXISTS Interpreters;
CREATE TABLE Interpreters (
	interpreter_uid int PRIMARY KEY  NOT NULL ,
	organization_uid int NOT NULL ,
	name varchar (128) NULL ,
	email text NULL 
) 
;

DROP TABLE IF EXISTS Locations;
CREATE TABLE Locations (
	location_uid int PRIMARY KEY  NOT NULL ,
	chr_start int NOT NULL ,
	chr_stop int NOT NULL 
) 
;

DROP TABLE IF EXISTS Locations_GroupVersions;
CREATE TABLE Locations_GroupVersions (
	location_uid int NOT NULL ,
	group_version_uid int NOT NULL ,
	chromosome varchar (8) NOT NULL 
) 
;

DROP TABLE IF EXISTS NextIds;
CREATE TABLE NextIds (
	table_name varchar (128) NOT NULL ,
	next_number int NOT NULL 
) 
;

DROP TABLE IF EXISTS Organizations;
CREATE TABLE Organizations (
	organization_uid int PRIMARY KEY  NOT NULL ,
	name varchar (128) NOT NULL ,
	approval_authority tinyint(1) NOT NULL 
) 
;

DROP TABLE IF EXISTS Programs;
CREATE TABLE Programs (
	program_uid int PRIMARY KEY  NOT NULL ,
	organization_uid int NOT NULL ,
	name text NOT NULL ,
	version text NULL 
) 
;


DROP TABLE IF EXISTS StatisticsTypes;
CREATE TABLE StatisticsTypes (
        statistics_type_uid int PRIMARY KEY  NOT NULL ,
        statistics_type varchar (20) NOT NULL 
) 
;

DROP TABLE IF EXISTS CcdsStatistics;
CREATE TABLE CcdsStatistics (
        statistics_uid int PRIMARY KEY NOT NULL ,
        statistics_type_uid int NOT NULL ,
        tax_id int NOT NULL ,
        ncbi_build_number int NOT NULL ,
        ncbi_build_version int NOT NULL ,
        statistics_html text NULL
) 
;

DROP TABLE IF EXISTS Builds;
CREATE TABLE Builds (
        build_uid int PRIMARY KEY NOT NULL ,
        tax_id int NOT NULL ,
        ncbi_build_number int NOT NULL ,
        ncbi_build_version int NOT NULL
) 
;

DROP TABLE IF EXISTS BuildQualityTests;
CREATE TABLE BuildQualityTests (
        build_uid int NOT NULL ,
        qa_analysis_id int NOT NULL ,
        is_required tinyint(1) NOT NULL ,
        acc_rejection_uid int NOT NULL ,
        val_description varchar (64) NULL
) 
;

