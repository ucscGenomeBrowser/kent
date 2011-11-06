/* encodeProject - javascript utilities for ENCODE-specific things 
 such as controlled vocabulary and experiment table

 Formatted: jsbeautify.py -j -k
 Syntax checked: jslint --indent=4 --plusplus=true --strict=false --browser=true
*/
/*global $ */

var encodeProject = (function () {
    var server = "genome.ucsc.edu",
        assembly = "hg19",
        cgi = "/cgi-bin/hgApi?";

    var accessionPrefix = 'wgEncodeE?';


    // TODO: modularize by extending Array.sort ?

    function cmpCV(a, b) {
        // Helper function for case-insensitive sort of CV objects
        //  Use label if any, otherwise the term
        A = (a.label !== undefined ? a.label.toUpperCase() : a.term.toUpperCase());
        B = (b.label !== undefined ? b.label.toUpperCase() : b.term.toUpperCase());
        if (A < B) {
            return -1;
        }
        if (A > B) {
            return 1;
        }
        return 0;
    }

    return {
        setup: function (settings) {
            // Change defaults
            if (settings.server) {
                server = settings.server;
            }
            if (settings.assembly) {
                assembly = settings.assembly;
            }
        },

        cmpNoCase: function (a, b) {
        // Helper function for case-insensitive sort - belongs in
        // more generic lib
            var A, B;
            A = a.toUpperCase();
            B = b.toUpperCase();
            if (A < B) {
                return -1;
            }
            if (A > B) {
                return 1;
            }
            return 0;
        },

        addSearchPanel: function (divId) {
            // Create panel of radio buttons for user to select search type
            // Add to passed in HTML div ID; e.g. #searchTypePanel
            return $(divId).append('<span id="searchPanelTitle"><strong>Search for:</strong></span><input type="radio" name="searchType" id="searchTracks" value="tracks" checked="checked">Tracks<input type="radio" name="searchType" id="searchFiles" value="files">Files');
        },

        getSearchUrl: function (assembly, vars) {
            // Return URL for search of type requested in search panel

            var prog, cartVar, url;
            if ($('input:radio[name=searchType]:checked').val() === "tracks") {
                prog = 'hgTracks';
                cartVar = 'hgt_tSearch';
            } else {
                prog = "hgFileSearch";
                cartVar = "hgfs_Search";
            }
             url = '/cgi-bin/' + prog + '?db=' + assembly + '&' + cartVar + '=search' +
                    '&tsCurTab=advancedTab&hgt_tsPage=';
            return (url);
        },

        getSearchType: function () {
            return $('input:radio[name=searchType]:checked').val();
        },

        getServer: function () {
            // Get currently set server 
            return server;
        },

        getDataGroups: function (dataTypes) {
            // Return sorted array of dataGroup objects each having a .label,
            // .dataTypes, 
            // and an array of dataTypes, alphasorted, with 'Other' last
            var dataGroupHash = {},
                dataGroups = [],
                otherGroup, group;
            $.each(dataTypes, function (i, dataType) {
                group = dataType.dataGroup;
                if (!group) {
                    return true;
                }
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
            dataGroups.sort(cmpCV);
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

        getCellTiers: function (cellTypes) {
            // Return sorted array of cellTier objects each having a .term,
            // with tier number, .celltypes, 
            // and an array of cell types, alphasorted
            var cellTiers = [],
                tier;
            $.each(cellTypes, function (i, cellType) {
                tier = cellType.tier;
                // ignore untiered cell types (all human should have a tier)
                if (!tier) {
                    return true;
                }
                if (!cellTiers[tier]) {
                    cellTiers[tier] = {
                        term: tier,
                        cellTypes: []
                    };
                }
                cellTiers[tier].cellTypes.push(cellType.term);
            });
            cellTiers.sort(cmpCV);
            $.each(cellTiers, function (i, tier) {
                if (!cellTiers[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                cellTiers[i].cellTypes.sort(encodeProject.cmpNoCase);
            });
            return cellTiers;
        },

        isHistone: function (target) {
            // Helper function, returns true if antibody target histone modification
            if (target === undefined) {
               return false;
            }
            return target.match(/^H[234]/);
        },

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

        getAntibodyGroups: function (antibodies) {
            // Return sorted array of antibodyGroup objects each having a .label
            // with group name (Histone Modification or Transcription Factor), 
            // and an array of antibody targets, alphasorted
            var antibodyGroups = [],
                antibodyGroupHash = {},
                group, target;
            $.each(antibodies, function (i, antibody) {
                group = encodeProject.isHistone(antibody.target) ? "Histone Modification" : "Transcription Factor";
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
            antibodyGroups.sort(cmpCV);
            $.each(antibodyGroups, function (i, group) {
                if (!antibodyGroups[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                antibodyGroups[i].targets.sort(encodeProject.cmpNoCase);
            });
            return antibodyGroups;
        },

        getExpIdHash: function (expIds) {
            // Return hash of experiment ID's
            var expIdHash = {};
            $.each(expIds, function (i, expId) {
                expIdHash[expId.expId] = true;
            });
            return expIdHash;
        },

        // UNTESTED
        expIdFromAccession: function(accession) {
            return accession.slice(accessionPrefix.length);
        },

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
                $.getJSON("http://" + server + cgi + "db=" + assembly + "&" + request, function (data) {
                    serverData[i] = data;
                    if (--count === 0) {
                        handler(serverData);
                    }
                });
            });
        }
    };
}());
