#!/bin/tcsh -efx
curl https://tracker-api.data.humancellatlas.org/v0/projects > parthProject.json
hcatParseParth parthProject.json parsedOut
