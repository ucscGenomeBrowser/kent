---------------------- Primary keys ------------------------

ALTER TABLE [dbo].[AccessionRejectionCriteria] ADD 
	CONSTRAINT [PK_AccessionRejectionCriteria] PRIMARY KEY  CLUSTERED 
	(
		[acc_rejection_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Accessions] ADD 
	CONSTRAINT [PK_Accessions] PRIMARY KEY  CLUSTERED 
	(
		[accession_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Accessions_GroupVersions] ADD 
	CONSTRAINT [PK_Accessions_GroupVersions] PRIMARY KEY  CLUSTERED 
	(
		[accession_uid],
		[group_version_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[CcdsStatusVals] ADD 
	CONSTRAINT [PK_CcdsStatusVals] PRIMARY KEY  CLUSTERED 
	(
		[ccds_status_val_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[CcdsUids] ADD 
	CONSTRAINT [PK_CcdsUids] PRIMARY KEY  CLUSTERED 
	(
		[ccds_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[ChromosomeAccessions] ADD 
	CONSTRAINT [PK_ChromosomeAccessions] PRIMARY KEY  CLUSTERED 
	(
		[chromosome_accession_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[GroupVersions] ADD 
	CONSTRAINT [PK_GroupVersions] PRIMARY KEY  CLUSTERED 
	(
		[group_version_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[GroupVersions_ChromosomeAccessions] ADD 
	CONSTRAINT [PK_GroupVersions_ChromosomeAccessions] PRIMARY KEY  CLUSTERED 
	(
		[group_version_uid],
		[chromosome_accession_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Groups] ADD 
	CONSTRAINT [PK_Groups] PRIMARY KEY  CLUSTERED 
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
	CONSTRAINT [PK_interpretationTypes] PRIMARY KEY  CLUSTERED 
	(
		[interpretation_type_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Interpretations] ADD 
	CONSTRAINT [PK_Interpretations] PRIMARY KEY  CLUSTERED 
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
	CONSTRAINT [PK_Locations] PRIMARY KEY  CLUSTERED 
	(
		[location_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Locations_GroupVersions] ADD 
	CONSTRAINT [PK_Locations_GroupVersions] PRIMARY KEY  CLUSTERED 
	(
		[location_uid],
		[group_version_uid],
                [chromosome]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[NextIds] ADD 
	CONSTRAINT [PK_NextIds] PRIMARY KEY  CLUSTERED 
	(
		[table_name]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
GO

ALTER TABLE [dbo].[Organizations] ADD 
	CONSTRAINT [PK_Organizations] PRIMARY KEY  CLUSTERED 
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

ALTER TABLE [dbo].[StatisticsTypes] ADD
        CONSTRAINT [PK_StatisticsTypes] PRIMARY KEY  CLUSTERED
        (
                [statistics_type_uid]
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[CcdsStatistics] ADD
        CONSTRAINT [PK_CcdsStatistics] PRIMARY KEY  CLUSTERED
        (
                [statistics_uid]
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[Builds] ADD
        CONSTRAINT [PK_Builds] PRIMARY KEY  CLUSTERED
        (
                [build_uid]
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[BuildQualityTests] ADD
        CONSTRAINT [PK_BuildQualityTests] PRIMARY KEY  CLUSTERED
        (
                [build_uid],
                [qa_analysis_id]
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[ProspectiveGroups] ADD
        CONSTRAINT [PK_ProspectiveGroups] PRIMARY KEY  CLUSTERED
        (
                [group_version_uid]
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[ProspectiveAnnotCompare] ADD
        CONSTRAINT [PK_ProspectiveAnnotCompare] PRIMARY KEY  CLUSTERED
        (
                [group_version_uid]
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[ReportTypes] ADD
        CONSTRAINT [PK_ReportTypes] PRIMARY KEY  CLUSTERED
        (
                [report_type_uid]
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[ReportQueries] ADD
        CONSTRAINT [PK_ReportQueries] PRIMARY KEY CLUSTERED
        (
                [query_uid] ASC
        ) WITH  FILLFACTOR = 90  ON [PRIMARY]
go

ALTER TABLE [dbo].[ProspectiveStatusVals] ADD 
	CONSTRAINT [PK_ProspectiveStatusVals] PRIMARY KEY  CLUSTERED 
	(
		[prospective_status_val_uid]
	) WITH  FILLFACTOR = 90  ON [PRIMARY] 
go

---------------------- Foreign keys ------------------------

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

ALTER TABLE [dbo].[GroupVersions] ADD
        CONSTRAINT [FK_GroupVersions_Builds] FOREIGN KEY
        (
                [build_uid]
        ) REFERENCES [dbo].[Builds] (
                [build_uid]
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

ALTER TABLE [dbo].[CcdsStatistics] ADD
        CONSTRAINT [FK_CcdsStatistics_StatisticsTypes] FOREIGN KEY
        (
                [statistics_type_uid]
        ) REFERENCES [dbo].[StatisticsTypes] (
                [statistics_type_uid]
        )
go

ALTER TABLE [dbo].[CcdsStatistics] ADD
        CONSTRAINT [FK_CcdsStatistics_Builds] FOREIGN KEY
        (
                [build_uid]
        ) REFERENCES [dbo].[Builds] (
                [build_uid]
        )

GO

ALTER TABLE [dbo].[Builds] ADD
        CONSTRAINT [FK_Builds_prev_build_uid] FOREIGN KEY
        (
                [prev_build_uid]
        ) REFERENCES [dbo].[Builds] (
                [build_uid]
        )
go

ALTER TABLE [dbo].[BuildQualityTests] ADD
        CONSTRAINT [FK_BuildQualityTests_Builds] FOREIGN KEY
        (
                [build_uid]
        ) REFERENCES [dbo].[Builds] (
                [build_uid]
        )
go

ALTER TABLE [dbo].[BuildQualityTests] ADD
        CONSTRAINT [FK_BuildQualityTests_AccessionRejectionCriteria] FOREIGN KEY
        (
                [acc_rejection_uid]
        ) REFERENCES [dbo].[AccessionRejectionCriteria] (
                [acc_rejection_uid]
        )
go

ALTER TABLE [dbo].[ProspectiveAnnotCompare] ADD
        CONSTRAINT [FK_ProspectiveAnnotCompare] FOREIGN KEY
        (
                [group_version_uid]
        ) REFERENCES [dbo].[ProspectiveGroups] (
                [group_version_uid]
        )
go

ALTER TABLE [dbo].[ProspectiveGroups] ADD
        CONSTRAINT [FK_ProspectiveGroups] FOREIGN KEY
        (
                [group_version_uid]
        ) REFERENCES [dbo].[GroupVersions] (
                [group_version_uid]
        )
go

ALTER TABLE [dbo].[ReportQueries] ADD
        CONSTRAINT [FK_ReportQueries_ReportTypes] FOREIGN KEY
        (       
                [report_type_uid]
        ) REFERENCES [dbo].[ReportTypes] (
                [report_type_uid]
        )
go

ALTER TABLE [dbo].[ProspectiveGroups] ADD
        CONSTRAINT [FK_ProspectiveGroups_Status] FOREIGN KEY
        (
                [prospective_status_val_uid]
        ) REFERENCES [dbo].[ProspectiveStatusVals] (
                [prospective_status_val_uid]
        )
go

