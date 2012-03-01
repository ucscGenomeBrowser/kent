/* encodeProject - 
        javascript utilities for ENCODE-specific things 
        such as controlled vocabulary and experiment table

 Formatted: jsbeautify.py -j -k
 Syntax checked: jslint --indent=4 --plusplus=true --strict=false --browser=true
*/
/*global $ */

var encodeProject = (function () {

    // Configurable variables - change with setup function below

    var server = "genome.ucsc.edu",
        assembly = "hg19",
        cgi = "/cgi-bin/hgApi?";

    var accessionPrefix = 'wgEncodeE?';
    var dataTypeLabelHash = {}, dataTypeTermHash = {}, cellTypeTermHash = {};

    // Functions

    return {

        // Configuration

        setup: function (settings) {
            // Change defaults
            if (settings.server) {
                server = settings.server;
            }
            if (settings.assembly) {
                assembly = settings.assembly;
            }
        },

        getAssembly: function () {
            // Get currently set assembly
            return assembly;
        },

        getServer: function () {
            // Get currently set server 
            return server;
        },

        // Server interaction

        serverRequests: {
            // Requests for data from server API
            experiment: "cmd=encodeExperiments",
            expId: "cmd=encodeExpId",
            dataType: "cmd=cv&type=dataType",
            cellType: "cmd=cv&type=cellType",
            antibody: "cmd=cv&type=antibody"
        },

        loadAllFromServer: function (requests, handler) {
            // Execute requests to server via ajax
            var serverData = [],
                count = requests.length;
            $.each(requests, function (i, request) {
                $.getJSON("http://" + server + cgi + "db=" + assembly + "&" + request, 
                    function (data) {
                        serverData[i] = data;
                        if (--count === 0) {
                            handler(serverData);
                        }
                });
            });
        },

        // Utility

        cmpNoCase: function (a, b) {
            // Case-insensitive sort.  
            // Should be in a generic lib
            return a.toLowerCase().localeCompare(b.toLowerCase());
        },

        cmpCV: function (a, b) {
            // Case-insensitive sort of CV objects
            //  Use label if any, otherwise the term
            return ((a.label !== undefined && b.label !== undefined) ? 
                a.label.toLowerCase().localeCompare(b.label.toLowerCase()) : 
                a.term.toLowerCase().localeCompare(b.term.toLowerCase()));
        },

        getSearchType: function () {
            return $('input:radio[name=searchType]:checked').val();
        },

        // Experiments, data types and cell types

        expIdFromAccession: function(accession) {
            return accession.slice(accessionPrefix.length);
        },

        getExpIdHash: function (ids) {
            // Return hash of experiment ID's

            var hash = {};
            $.each(ids, function (i, id) {
                hash[id.expId] = true;
            });
            return hash;
        },

        getDataType: function (term) {
            // Return cellType object from term
            // Needs loader function (using getDataGroups below for now)
            if (dataTypeTermHash !== undefined && 
                dataTypeTermHash[term] !== undefined) {
                    return dataTypeTermHash[term];
            }
            return null;
        },

        getDataTypeByLabel: function (label) {
            // Return hash of dataTypes keyed by term
            // Needs loader function (using getDataGroups below for now)
            if (dataTypeLabelHash !== undefined && 
                dataTypeLabelHash[label] !== undefined) {
                    return dataTypeLabelHash[label];
            }
        },

        getDataGroups: function (dataTypes) {
            // Unpack JSON list of dataTypes
            // Return sorted array of dataGroup objects each having a .label,
            // .dataTypes, 
            // and an array of dataTypes, alphasorted, with 'Other' last
            // Also populates hashes for lookup by term or label (for now)

            var dataGroupHash = {},
                dataGroups = [],
                otherGroup, group;
            $.each(dataTypes, function (i, dataType) {
                group = dataType.dataGroup;
                if (!group) {
                    return true;
                }
                // stash hashes for lookup by utility functions
                dataTypeTermHash[dataType.term] = dataType;
                dataTypeLabelHash[dataType.label] = dataType;
                if (!dataGroupHash[group]) {
                    dataGroupHash[group] = {
                        label: group,
                        dataTypes: []
                    };
                }
                dataGroupHash[group].dataTypes.push(dataType.label);
            });
            $.each(dataGroupHash, function (key, item) {
                if (key === "Other") {
                    otherGroup = item;
                } else {
                    dataGroups.push(item);
                }
            });
            dataGroups.sort(encodeProject.cmpCV);
            dataGroups.push(otherGroup);
            $.each(dataGroups, function (i, group) {
                if (!dataGroups[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                dataGroups[i].dataTypes.sort(encodeProject.cmpNoCase);
            });
            return dataGroups;
        },

        getCellType: function (cellType) {
            // Return cellType object from term
            // Needs loader function (using getCellTiers below for now)
            if (cellTypeTermHash !== undefined && 
                cellTypeTermHash[cellType] !== undefined) {
                    return cellTypeTermHash[cellType];
            }
            return null;
        },

        getCellTiers: function (cellTypes) {
            // Unpack JSON list of cellTypes
            // Return sorted array of cellTier objects each having a .term,
            // with tier number, .celltypes, and an array of cell types, alphasorted
            // Also loads hash for lookup by term (for now)
            var cellTiers = [],
                tier;
            $.each(cellTypes, function (i, cellType) {
                tier = cellType.tier;
                // ignore untiered cell types (all human should have a tier)
                if (!tier) {
                    return true;
                }
                cellTypeTermHash[cellType.term] = cellType;
                if (!cellTiers[tier]) {
                    cellTiers[tier] = {
                        term: tier,
                        cellTypes: []
                    };
                }
                cellTiers[tier].cellTypes.push(cellType.term);
            });
            cellTiers.sort(encodeProject.cmpCV);
            $.each(cellTiers, function (i, tier) {
                if (!cellTiers[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                cellTiers[i].cellTypes.sort(encodeProject.cmpNoCase);
            });
            return cellTiers;
        },

        // Antibodies

        antibodyFromExp: function (experiment) {
            // Get antibody from expVars field of experiment
            var match = experiment.expVars.match(/antibody=(\S+)/);
            if (match) {
                return match[1];
            }
        },

        targetFromAntibody: function (antibody, antibodyCV) {
            // Get target for antibody from antibody controlled vocab
            if (antibodyCV[antibody]) {
                return antibodyCV[antibody].target;
            }
        },

        isHistone: function (target) {
            // Helper function, returns true if antibody target histone modification
            if (target === undefined) {
               return false;
            }
            return target.match(/^H[234]/);
        },

        getAntibodyGroups: function (antibodies) {
            // Return sorted array of antibodyGroup objects each having a .label
            // with group name (Histone Modification or Transcription Factor), 
            // and an array of antibody targets, alphasorted
            var antibodyGroups = [],
                antibodyGroupHash = {},
                group, target;
            $.each(antibodies, function (i, antibody) {
                group = encodeProject.isHistone(antibody.target) ? 
                        "Histone Modification" : "Transcription Factor";
                if (!antibodyGroupHash[group]) {
                    antibodyGroupHash[group] = {
                        label: group,
                        targets: []
                    };
                }
                antibodyGroupHash[group].targets.push(antibody.target);
            });
            $.each(antibodyGroupHash, function (key, item) {
                antibodyGroups.push(item);
            });
            antibodyGroups.sort(encodeProject.cmpCV);
            $.each(antibodyGroups, function (i, group) {
                if (!antibodyGroups[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                antibodyGroups[i].targets.sort(encodeProject.cmpNoCase);
            });
            return antibodyGroups;
        }
    };
}());
