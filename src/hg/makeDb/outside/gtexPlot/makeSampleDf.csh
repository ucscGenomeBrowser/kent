hgsql hgFixed -e 'select sampleId, gtexTissue.description as tissue, gtexTissue.organ as organ, gender, age, gtexDonor.name as donor, deathClass, ischemicTime, autolysisScore, rin, collectionSites, batchId, isolationDate from gtexSample, gtexDonor, gtexTissue where gtexSample.donor=gtexDonor.name and gtexSample.tissue=gtexTissue.name' | sed -e 's/(.*)//'

