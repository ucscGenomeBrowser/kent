ALTER TABLE [dbo].[AccessionRejectionCriteria] ADD 
	CONSTRAINT [PK_AccessionRejectionCriteria] PRIMARY KEY  CLUSTERED 
	(
		[acc_rejection_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Accessions] ADD 
	CONSTRAINT [PK_members] PRIMARY KEY  CLUSTERED 
	(
		[accession_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Accessions_GroupVersions] ADD 
	CONSTRAINT [PK_members_groupVersions] PRIMARY KEY  CLUSTERED 
	(
		[accession_uid],
		[group_version_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[CcdsStatusVals] ADD 
	CONSTRAINT [PK_review_category_vals] PRIMARY KEY  CLUSTERED 
	(
		[ccds_status_val_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[CcdsUids] ADD 
	CONSTRAINT [PK_ccdsIds] PRIMARY KEY  CLUSTERED 
	(
		[ccds_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[ChromosomeAccessions] ADD 
	CONSTRAINT [PK_chromosome_identities] PRIMARY KEY  CLUSTERED 
	(
		[chromosome_accession_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[GroupVersions] ADD 
	CONSTRAINT [PK_group_versions] PRIMARY KEY  CLUSTERED 
	(
		[group_version_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[GroupVersions_ChromosomeAccessions] ADD 
	CONSTRAINT [PK_chromosomes] PRIMARY KEY  CLUSTERED 
	(
		[group_version_uid],
		[chromosome_accession_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Groups] ADD 
	CONSTRAINT [PK_cds_groups] PRIMARY KEY  CLUSTERED 
	(
		[group_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[InterpretationSubtypes] ADD 
	CONSTRAINT [PK_InterpretationSubtypes] PRIMARY KEY  CLUSTERED 
	(
		[interpretation_subtype_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[InterpretationTypes] ADD 
	CONSTRAINT [PK_interpretationTypeVals] PRIMARY KEY  CLUSTERED 
	(
		[interpretation_type_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Interpretations] ADD 
	CONSTRAINT [PK_interpretations] PRIMARY KEY  CLUSTERED 
	(
		[interpretation_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Interpreters] ADD 
	CONSTRAINT [PK_Interpreters] PRIMARY KEY  CLUSTERED 
	(
		[interpreter_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Locations] ADD 
	CONSTRAINT [PK_locations] PRIMARY KEY  CLUSTERED 
	(
		[location_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Locations_GroupVersions] ADD 
	CONSTRAINT [PK_locations_groupVersions] PRIMARY KEY  CLUSTERED 
	(
		[location_uid],
		[group_version_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[NextIds] ADD 
	CONSTRAINT [next_id_table] PRIMARY KEY  CLUSTERED 
	(
		[table_name]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Organizations] ADD 
	CONSTRAINT [PK_member_type_vals] PRIMARY KEY  CLUSTERED 
	(
		[organization_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Programs] ADD 
	CONSTRAINT [PK_Programs] PRIMARY KEY  CLUSTERED 
	(
		[program_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Accessions] ADD 
	CONSTRAINT [FK_Accessions_Organizations] FOREIGN KEY 
	(
		[organization_uid]
	) REFERENCES [dbo].[Organizations] (
		[organization_uid]
	)
GO

ALTER TABLE [dbo].[Accessions_GroupVersions] ADD 
	CONSTRAINT [FK_Accessions_groupVersions_Accessions] FOREIGN KEY 
	(
		[accession_uid]
	) REFERENCES [dbo].[Accessions] (
		[accession_uid]
	),
	CONSTRAINT [FK_Accessions_groupVersions_CcdsStatusVals] FOREIGN KEY 
	(
		[ccds_status_val_uid]
	) REFERENCES [dbo].[CcdsStatusVals] (
		[ccds_status_val_uid]
	),
	CONSTRAINT [FK_Accessions_groupVersions_GroupVersions] FOREIGN KEY 
	(
		[group_version_uid]
	) REFERENCES [dbo].[GroupVersions] (
		[group_version_uid]
	)
GO

ALTER TABLE [dbo].[CcdsUids] ADD 
	CONSTRAINT [FK_CcdsUids_Groups] FOREIGN KEY 
	(
		[group_uid]
	) REFERENCES [dbo].[Groups] (
		[group_uid]
	)
GO

ALTER TABLE [dbo].[ChromosomeAccessions] ADD 
	CONSTRAINT [FK_ChromosomeAccessionss_Organizations] FOREIGN KEY 
	(
		[organization_uid]
	) REFERENCES [dbo].[Organizations] (
		[organization_uid]
	)
GO

ALTER TABLE [dbo].[GroupVersions] ADD 
	CONSTRAINT [FK_GroupVersions_CcdsStatuses] FOREIGN KEY 
	(
		[ccds_status_val_uid]
	) REFERENCES [dbo].[CcdsStatusVals] (
		[ccds_status_val_uid]
	),
	CONSTRAINT [FK_GroupVersions_Groups] FOREIGN KEY 
	(
		[group_uid]
	) REFERENCES [dbo].[Groups] (
		[group_uid]
	)
GO

ALTER TABLE [dbo].[GroupVersions_ChromosomeAccessions] ADD 
	CONSTRAINT [FK_GroupVersions_chromosomeAccessions_ChromosomeAccessions] FOREIGN KEY 
	(
		[chromosome_accession_uid]
	) REFERENCES [dbo].[ChromosomeAccessions] (
		[chromosome_accession_uid]
	),
	CONSTRAINT [FK_GroupVersions_chromosomeIdentifiers_GroupVersions] FOREIGN KEY 
	(
		[group_version_uid]
	) REFERENCES [dbo].[GroupVersions] (
		[group_version_uid]
	)
GO

ALTER TABLE [dbo].[InterpretationSubtypes] ADD 
	CONSTRAINT [FK_InterpretationSubtypes_InterpretationTypes] FOREIGN KEY 
	(
		[interpretation_type_uid]
	) REFERENCES [dbo].[InterpretationTypes] (
		[interpretation_type_uid]
	)
GO

ALTER TABLE [dbo].[Interpretations] ADD 
	CONSTRAINT [FK_Interpretations_AccessionRejectionCriteria] FOREIGN KEY 
	(
		[acc_rejection_uid]
	) REFERENCES [dbo].[AccessionRejectionCriteria] (
		[acc_rejection_uid]
	),
	CONSTRAINT [FK_Interpretations_Accessions] FOREIGN KEY 
	(
		[accession_uid]
	) REFERENCES [dbo].[Accessions] (
		[accession_uid]
	),
	CONSTRAINT [FK_Interpretations_CcdsUids] FOREIGN KEY 
	(
		[ccds_uid]
	) REFERENCES [dbo].[CcdsUids] (
		[ccds_uid]
	),
	CONSTRAINT [FK_Interpretations_GroupVersions] FOREIGN KEY 
	(
		[group_version_uid]
	) REFERENCES [dbo].[GroupVersions] (
		[group_version_uid]
	),
	CONSTRAINT [FK_Interpretations_Groups] FOREIGN KEY 
	(
		[group_uid]
	) REFERENCES [dbo].[Groups] (
		[group_uid]
	),
	CONSTRAINT [FK_Interpretations_InterpretationSubtypes] FOREIGN KEY 
	(
		[interpretation_subtype_uid]
	) REFERENCES [dbo].[InterpretationSubtypes] (
		[interpretation_subtype_uid]
	),
	CONSTRAINT [FK_Interpretations_InterpretationTypes] FOREIGN KEY 
	(
		[interpretation_type_uid]
	) REFERENCES [dbo].[InterpretationTypes] (
		[interpretation_type_uid]
	),
	CONSTRAINT [FK_Interpretations_Interpretations] FOREIGN KEY 
	(
		[parent_interpretation_uid]
	) REFERENCES [dbo].[Interpretations] (
		[interpretation_uid]
	),
	CONSTRAINT [FK_Interpretations_Interpreters] FOREIGN KEY 
	(
		[interpreter_uid]
	) REFERENCES [dbo].[Interpreters] (
		[interpreter_uid]
	),
	CONSTRAINT [FK_Interpretations_Programs] FOREIGN KEY 
	(
		[program_uid]
	) REFERENCES [dbo].[Programs] (
		[program_uid]
	)
GO

ALTER TABLE [dbo].[Interpreters] ADD 
	CONSTRAINT [FK_Interpreters_Organizations] FOREIGN KEY 
	(
		[organization_uid]
	) REFERENCES [dbo].[Organizations] (
		[organization_uid]
	)
GO

ALTER TABLE [dbo].[Locations_GroupVersions] ADD 
	CONSTRAINT [FK_Locations_groupVersions_GroupVersions] FOREIGN KEY 
	(
		[group_version_uid]
	) REFERENCES [dbo].[GroupVersions] (
		[group_version_uid]
	),
	CONSTRAINT [FK_Locations_groupVersions_Locations] FOREIGN KEY 
	(
		[location_uid]
	) REFERENCES [dbo].[Locations] (
		[location_uid]
	)
GO

ALTER TABLE [dbo].[Programs] ADD 
	CONSTRAINT [FK_Programs_Organizations] FOREIGN KEY 
	(
		[organization_uid]
	) REFERENCES [dbo].[Organizations] (
		[organization_uid]
	)
GO

