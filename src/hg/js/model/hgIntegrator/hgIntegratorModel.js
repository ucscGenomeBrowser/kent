/* global AppComponent, ImModel, CladeOrgDbMixin, PositionSearchMixin, UserRegionsMixin */

var HgIntegratorModel = ImModel.extend({
    // Handle hgIntegrator's UI state and events and interaction with the server/CGI/cart.

    mixins: [ CladeOrgDbMixin(['cladeOrgDb']),
              PositionSearchMixin(['regionSelect', 'positionInfo']),
              UserRegionsMixin(['regionSelect', 'userRegions'])],

    // Hardcoded params / constants
    maxDataSources: 5,
    maxRelatedTables: 4,
    tdbFields: 'track,table,shortLabel,parent,subtracks,noGenome',
    excludeTypes: 'bam,wigMaf,maf',

    // Cart variables
    querySpecVar: 'hgi_querySpec',
    uiChoicesVar: 'hgi_uiChoices',
    rangeSelectVar: 'hgi_range', //#*** TODO: fold into uiChoices!
    addDsSelectVar: 'hgi_addDsTrackPath', //#*** TODO: fold into uiChoices!

    handleUserRegionsUpdate: function(mutState, cartVar, newValue) {
        var range;
        // User-defined regions updated -- if nonempty, change range to userRegions
        if (cartVar === 'userRegionsUpdate') {
            if (newValue && newValue !== "") {
                mutState.setIn(['regionSelect', this.rangeSelectVar], 'userRegions');
            }
        } else if (cartVar === 'userRegions') {
            // If we just changed db and there are no defined regions for this db,
            // hgi_range can't be 'userRegions'.
            range = mutState.getIn(['regionSelect', this.rangeSelectVar]);
            if ((! newValue || newValue === '') && range === 'userRegions') {
                this.clearUserRegions(mutState);
            }
        } else {
            this.error('handleUserRegionsUpdate: unexpectedly given', cartVar, newValue);
        }
    },

    querySpecUpdateUiChoices: function(mutState) {
        // Incoming dataSources[*].relatedTables overrides uiChoices relatedSelected and.
        // uiChoices tableFields[track][table][fields] for related tables.
        // Incoming outFileOptions.tableFields overrides uiChoices tableFields[track][track].
        // For each dataSource, override uiChoices or remove if not provided.
        var tableFields;
        mutState.get('dataSources').forEach(function(ds, dsIx) {
            // Extract this dataSource's related table info.
            var track = ds.get('trackPath').last();
            var relatedTables = ds.getIn(['config', 'relatedTables']);
            var relSel;
            if (relatedTables && relatedTables.size > 0) {
                // The query does include related tables & fields -- update uiChoices.
                relSel = relatedTables.map(function(info) { return info.get('table'); });
                this.setUiChoice(mutState, ['relatedSelected', track], relSel);
                relatedTables.forEach(
                    function (info) {
                        var table = info.get('table');
                        var fields = info.get('fields') || Immutable.List();
                        this.setUiChoice(mutState, ['relatedTableFields', track, table], fields);
                    },
                this);
            }
            // Remove from ds to avoid confusion; uiChoices owns the info now.
            mutState.removeIn(['dataSources', dsIx, 'config', 'relatedTables']);
        }, this);
        // outFileOptions.tableFields may contain deselected fields from (main tables of) sources.
        tableFields = mutState.getIn(['outFileOptions', 'tableFields']);
        if (tableFields) {
            tableFields.forEach(
                function(fields, track) {
                    this.setUiChoice(mutState, ['tableFields', track], fields);
                },
                this);
        }
        this.cartUpdateUiChoices(mutState, this.uiChoicesVar);
    },

    regionIsGenome: function(mutState) {
        // Return true if the user has selected genome as the query region.
        return (mutState.getIn(['regionSelect', this.rangeSelectVar]) === 'genome');
    },

    dataSourceIsNoGenome: function(mutState, ds) {
        // Return true if the trackDb entry for ds has noGenome.
        var trackPath = ds.get('trackPath');
        var track = this.findGroupedTrack(mutState, trackPath);
        // track might be null, e.g. if ds is carried over from some other db
        return track && track.get('noGenome');
    },

    checkNoGenome: function(mutState) {
        // If one of the dataSources has the tdb setting 'tableBrowser noGenome',
        // disable the region option for genome, and if genome is currently selected
        // as the region, force it to position.
        var regionIsGenome = this.regionIsGenome(mutState);
        var dataSources = mutState.get('dataSources') || Immutable.List();
        var includesNoGenome = dataSources.some(function(ds) {
            return this.dataSourceIsNoGenome(mutState, ds);
        }, this);
        mutState.setIn(['regionSelect', 'disableGenome'], includesNoGenome);
        if (regionIsGenome && includesNoGenome) {
            mutState.setIn(['regionSelect', this.rangeSelectVar], 'position');
            this.cartSet(this.rangeSelectVar, 'position');
        }
    },

    handleQueryState: function(mutState, cartVar, queryState) {
        // The querySpec from cart, received only when the page is initially displaying,
        // requires special handling because portions of it become top-level UI state,
        // and parts of querySpec can override parts of uiChoices.
        var querySpec, uiChoices;
        if (cartVar === 'queryState') {
            querySpec = this.parseCartJsonString(queryState[this.querySpecVar]);
            uiChoices = this.parseCartJsonString(queryState[this.uiChoicesVar]);
            if (querySpec) {
                // Overwriting dataSources blows away some UI state like labels;
                // Those will be added back in this.validateCart.
                mutState.set('dataSources', Immutable.fromJS(querySpec.dataSources));
                mutState.set('outFileOptions', Immutable.fromJS(querySpec.outFileOptions));
            } else {
                // No querySpec in cart yet -- make an empty one for rendering:
                mutState.set('dataSources', Immutable.List());
                mutState.set('outFileOptions', Immutable.Map());
            }
            if (uiChoices) {
                this.setUiChoice(mutState, [], Immutable.fromJS(uiChoices));
            }
            this.querySpecUpdateUiChoices(mutState);
            this.checkNoGenome(mutState);
        } else {
            this.error('handleQueryState: unexpectedly given', cartVar, queryState);
        }
    },

    handleRangeSelect: function(mutState, cartVar, newValue) {
        // Bundle this with positionInfo for fast immutable-prop checking
        if (cartVar === this.rangeSelectVar) {
            mutState.setIn(['regionSelect', cartVar], newValue);
            mutState.setIn(['regionSelect', 'loading'], false);
        } else {
            this.error('handleRangeSelect: unexpectedly given', cartVar, newValue);
        }
    },

    fieldCheckedByDefault: function(table, field) {
        // Return true if checkbox for including table field in output would be checked by default.
        // If table contains a '.' (db.table), it's a related SQL table; no fields checked by
        // default.  Otherwise, it's the main data source; every field except bin is checked.
        return (!_.includes(table, '.') && field !== 'bin');
    },

    getRelatedAvailableSelected: function (mutState, track, optionList) {
        // Determine which related table in optionList is selected, based on past choices,
        // availability and defaults for popular tracks.
        var relatedTables = optionList.map(function (option) { return option.get('value'); })
                                      .toArray();
        var selected = this.getUiChoice(mutState,
                                        ['relatedAvailable', track, 'selected']);
        if (!_.includes(relatedTables, selected)) {
            selected = undefined;
        }
        if (_.isUndefined(selected)) {
            // Pick useful defaults for tracks with tons of related tables.
            if (track === 'knownGene') {
                selected = _.find(relatedTables,
                                  function (relTable) { return _.endsWith(relTable, '.kgXref'); });
            } else if (track === 'refGene') {
                selected = _.find(relatedTables,
                                  function (relTable) { return _.endsWith(relTable, '.refLink'); });
            }
        }
        if (_.isUndefined(selected)) {
            selected = relatedTables[0];
        }
        return selected;
    },

    getTablesIncluded: function (mutState) {
        // Return an object whose keys are tables that have been selected in uiChoices
        // (so we can exclude them from menus of related tables that could be added)
        // First make list of all tracks' tables:
        var tablesIncluded = _.flatten(mutState.get('fieldSelect').map(
            function(trackInfo) {
                return trackInfo.get('tableFields')
                                .map(function(x, table) { return table; }).toArray();
            }).toArray());
        // Transform list into an into an object with table names as keys for easy testing:
        return _.countBy(tablesIncluded);
    },

    updateRelatedTablesMenus: function (mutState) {
        // Each track's menu of not-yet-selected related tables, if any.
        // They are interrelated because selecting a related table for one track
        // means we need to disable that related table in all tracks that link to it.
        // Disable the 'Add' button if they have already selected our max number
        // of related tables or if the selected table has already been added.
        var tablesIncluded = this.getTablesIncluded(mutState);
        mutState.get('fieldSelect').forEach(
            function (trackState, track) {
                var optionList = trackState.getIn(['relatedAvailable', 'options']);
                var selectedTable, tableFields, disableAddButton;
                if (optionList) {
                    selectedTable = this.getRelatedAvailableSelected(mutState, track, optionList);
                    tableFields = trackState.get('tableFields');
                    disableAddButton = (tableFields.size - 1 >= this.maxRelatedTables);
                    optionList.forEach(
                        function (option, opIx) {
                            var relTable = option.get('value');
                            var disableIt = (!!tablesIncluded[relTable] ||
                                             (option.get('noGenome') &&
                                              this.regionIsGenome(mutState)));
                            mutState.setIn(['fieldSelect', track, 'relatedAvailable', 'options',
                                            opIx, 'disabled'],
                                           disableIt);
                            if (disableIt && relTable === selectedTable) {
                                disableAddButton = true;
                            }
                        },
                        this);
                    // Set selected relTable
                    mutState.setIn(['fieldSelect', track, 'relatedAvailable', 'selected'],
                                   selectedTable);
                    mutState.setIn(['fieldSelect', track, 'disableAddButton'],
                                   disableAddButton);
                }
            },
            this);
    },

    fieldIsCheckedInUiChoices: function(mutState, track, table, field) {
        // Figure out the right path in uiChoices for track/table/field, look for a value
        // there, and return it if found.  Otherwise return the default for table/field.
        // For backwards compatibility, the main table's fields are stored as a
        // Map: field -> boolean isChecked, and in practice only the fields with
        // non-default values are stored.  Fields of related tables are stored
        // as lists of non-default (i.e. checked) fields.
        var checkedByDefault = this.fieldCheckedByDefault(table, field);
        var checked, nonDefaultFields;
        if (table === track) {
            // Value in map or default
            checked = this.getUiChoice(mutState,
                                       ['tableFields', track, field]);
            if (_.isUndefined(checked)) {
                checked = checkedByDefault;
            }
        } else {
            // Presence in list means that field has the non-default value:
            nonDefaultFields = this.getUiChoice(mutState, ['relatedTableFields', track, table]);
            if (nonDefaultFields &&
                nonDefaultFields.some(function (f) { return f === field; })) {
                checked = !checkedByDefault;
            } else {
                checked = checkedByDefault;
            }
        }
        return checked;
    },

    mergeFieldSettings: function(mutState, tableFieldsFromServer) {
        // Merge data from a fieldSelect or relatedFields server response into the
        // fieldSelect UI obj.
        // tableFieldsFromServer settings are plain JS:
        // { <track>: { <table>: { label: ___,
        //                         fields: [ { name: ___, desc: ___ }, ... ],
        //                         noGenome: true|false
        //                       },
        //                       ... },
        //            }, ... }
        _.forEach(tableFieldsFromServer,
            function(trackTables, track) {
                // Each track has info for one or more tables, and possibly also related tables
                _.forEach(trackTables,
                    function(tableInfo, table) {
                        // Each table has a label, a list of objects w/field name and description,
                        // and a (usually empty) list of related db.table names
                        var fsPathToTable = ['fieldSelect', track, 'tableFields', table];
                        if (tableInfo.isNoGenome && this.regionIsGenome(mutState)) {
                            return;
                        }
                        mutState.setIn(fsPathToTable.concat('label'), tableInfo.label);
                        // Add each field's checkbox state for UI
                        mutState.setIn(fsPathToTable.concat('fields'), Immutable.List());
                        _.forEach(tableInfo.fields,
                            function(fieldAndDesc, ix) {
                                var field = fieldAndDesc.name;
                                var checked = this.fieldIsCheckedInUiChoices(mutState,
                                                                             track, table, field);
                                var newSetting = Immutable.Map({ name: field,
                                                                 checked: checked,
                                                                 desc: fieldAndDesc.desc });
                                mutState.setIn(fsPathToTable.concat(['fields', ix]), newSetting);
                            },
                        this);
                    },
                this);
            },
        this);
    },

    handleFieldSelect: function(mutState, cartVar, tableFields) {
        // The server has sent an object mapping data source names to table names to
        // lists of fields.
        // Construct an immutable map of data source names to maps of table names to
        // ordered maps of fields to booleans & descriptions for checkboxes.
        // If uiChoices contains saved settings, use those; otherwise true unless field is bin.
        if (cartVar === 'fieldSelect') {
            // New list of track tables; build popup contents from scratch.
            mutState.set('fieldSelect', Immutable.OrderedMap());
            this.mergeFieldSettings(mutState, tableFields);
        } else {
            this.error('handleTableFields: unexpectedly given', cartVar, tableFields);
        }
    },

    handleRelatedFields: function(mutState, cartVar, tableFields) {
        // The server has sent an object mapping data source names to related SQL table names
        // to lists of fields.
        // Construct an immutable map of table names to ordered maps of fields to
        // booleans for checkboxes; if uiChoices contains saved settings,
        // use those; otherwise true unless field is bin.
        if (cartVar === 'relatedFields') {
            // Related tables to add; merge them into the existing Modal state.
            if (! mutState.get('fieldSelect')) {
                // The user must have closed the Modal before these arrived.
                return;
            }
            this.mergeFieldSettings(mutState, tableFields);
        } else {
            this.error('handleRelatedFields: unexpectedly given', cartVar, tableFields);
        }
    },

    handleRelatedTables: function(mutState, cartVar, relatedTables) {
        // The server has sent an object mapping data source names to lists of
        // available related tables (which can change every time another related
        // table is selected).  Update state and refresh all tracks' menus
        // of related tables.
        if (cartVar === 'relatedTablesUpdate' && ! mutState.get('fieldSelect')) {
            // The user must have closed the Modal before these arrived.
            return;
        } else if (cartVar === 'relatedTablesAll' || cartVar === 'relatedTablesUpdate') {
            _.forEach(relatedTables,
                // Merge new lists into state: rebuild each track's related tables menu option list.
                function(tables, track) {
                    var options = _.map(tables,
                        function(tblDescTuple) {
                            var relTable = tblDescTuple[0];
                            var label = tblDescTuple[1];
                            var noGenome = tblDescTuple[2];
                            // Don't set disabled until we know all tables included in the query.
                            return { value: relTable, label: label, noGenome: noGenome };
                        });
                    mutState.setIn(['fieldSelect', track, 'relatedAvailable', 'options'],
                                   Immutable.fromJS(options));
                }
            );
        this.updateRelatedTablesMenus(mutState);
        } else {
            this.error('handleRelatedTables: unexpectedly given', cartVar, relatedTables);
        }
    },

    getAddDsTrackPath: function(mutState) {
        // Return Immutable.List of currently selected items in the menus for adding a data source.
        // That serves as a path through group, track, and maybe view and/or subtrack.
        // If menus haven't been built yet, returns empty Immutable.List.
        var addDsMenus = mutState.getIn(['addDsInfo', 'menus']) || Immutable.List();
        return addDsMenus.map(function(menuInfo) { return menuInfo.get('selected'); });
    },

    findGroupedTrack: function(mutState, trackPath) {
        // Return the group or track object found by following trackPath through groupedTrackDb,
        // or null.
        var objList = mutState.get('groupedTrackDb');
        var selectedObj = null;
        trackPath.forEach(function(selected, ix) {
            var valueField = (ix === 0) ? 'name' : 'track';
            var childField = (ix === 0) ? 'tracks' : 'subtracks';
            selectedObj = objList ? this.findObjByField(objList, valueField, selected) : null;
            if (! selectedObj) {
                return false;
            }
            objList = selectedObj.get(childField);
        }, this);
        return selectedObj;
    },

    isInGroupedTrackDb: function(mutState, trackPath) {
        // Search for trackPath in groupedTrackDb; return true if found.
        return this.findGroupedTrack(mutState, trackPath) ? true : false;
    },

    schemaUrlFromTrackPath: function(mutState, db, trackPath) {
        // Formulate a link to hgTables' table schema for trackPath's group/track/table.
        var group = trackPath.first();
        var track = trackPath.get(1);
        // table is almost always the same as leaf track -- but not always, for example
        // track wgEncodeRegDnaseClustered, table wgEncodeRegDnaseClusteredV3.
        var table = null;
        var leafObj = this.findGroupedTrack(mutState, trackPath);
        if (leafObj) {
            table = leafObj.get('table');
            return 'hgTables?db=' + db + '&hgta_group=' + group + '&hgta_track=' + track +
                                '&hgta_table=' + table + '&hgta_doSchema=1';
        } else {
            if (trackPath.size > 0 && trackPath.get(0)) {
                this.error('schemaUrlFromTrackPath: can\'t find trackDb for trackPath ' +
                           trackPath.toString());
            }
            return null;
        }
    },

    findObjByField: function(objList, field, value) {
        // Return the first Immutable object from Immutable.List objList whose value of field
        // matches the given value, if there is one.
        return objList.find(function(obj) {
            return obj.get(field) === value;
        });
    },

    findObjByFieldOrFirst: function(objList, field, selectedValue) {
        // Use use selectedValue to identify the selected object by field if possible;
        // otherwise just return the first object.
        var selectedObj;
        if (! selectedValue ||
            ! (selectedObj =
               this.findObjByField(objList, field, selectedValue))) {
            selectedObj = objList.first();
        }
        return selectedObj;
    },

    groupLabel: function(groupObj) {
        return groupObj.get('label');
    },

    trackLabel: function(trackObj) {
        // Make a nice label for a track, fitting in extra info for superTrack children
        // or leaves.
        var label = trackObj.get('shortLabel');
        var parent = trackObj.get('parent');
        var parentLabel;
        var subtracks = trackObj.get('subtracks');
        if (parent && Immutable.Map.isMap(parent)) {
            // parent is a superTrack -- add its shortLabel unless redundant
            parentLabel = parent.get('shortLabel');
            if (! _.startsWith(label, parentLabel)) {
                label = parentLabel + ' - ' + label;
            }
        }
        if (subtracks) {
            label += '...';
        } else {
            label += ' (' + trackObj.get('table') + ')';
        }
        return label;
    },

    makeMenu: function(objList, selectedObj, label, valueField, optionLabelFunc, hidden) {
        // Return an Immutable menu descriptor object suitable for <LabeledSelectRow>.
        // objList must be an Immutable.List of Immutable objects that have valueField.
        // optionLabelFunc, when called on a member of objList, returns a string.
        // selectedObj must be a member of objList or falsey.
        // For now, include the noGenome flag but don't set disabled -- that would be overwritten
        // later in disableDataSourcesInAddDsMenus.
        var valLabels = objList.map(function(obj) {
            return Immutable.Map({ value: obj.get(valueField),
                                   label: optionLabelFunc(obj),
                                   noGenome: obj.get('noGenome')
                                 });
        }, this);
        var selected = selectedObj ? selectedObj.get(valueField) : null;
        return Immutable.fromJS({ valLabels: valLabels,
                                  selected: selected,
                                  label: label,
                                  hide: hidden });
    },

    updateAddDsMenu: function(mutState, changedIx, ix, objList, selectedValue,
                              menuLabel, valueField, optionLabelFunc, childField) {
        // If ix >= changedIx, or if selectedValue doesn't match the selected value of the
        // ixth level of addDsMenus, then rebuild the ixth level of addDsMenus and truncate
        // any member(s) of addDsMenus past ix.
        // If selectedValue can't be found in objList, use the first obj in objList.
        // If there's only one trackObj at a non-leaf level, don't make a menu for that level.
        // Return the selected member of objList so its children can be used to form the
        // next level of addDsMenus.
        var selectedObj = this.findObjByFieldOrFirst(objList, valueField, selectedValue);
        var atLeaf = ! (selectedObj && selectedObj.get(childField));
        var addDsMenus = mutState.getIn(['addDsInfo', 'menus']) || Immutable.List();
        var hidden = false;
        if (changedIx <= ix ||
            addDsMenus.getIn([ix, 'selected']) !== selectedObj.get(valueField)) {
            // If this level is changing, so are the levels below it.  Rebuild addDsMenus[ix]
            // and truncate anything after ix.
            // Tweak label if it's a top-level track or view-in-the-middle not a subtrack:
            if (menuLabel === 'subtrack') {
                if (ix === 1) {
                    menuLabel = 'track';
                } else if (ix === 2 && !atLeaf) {
                    menuLabel = 'view';
                }
            }
            // If this is a non-leaf level (e.g. view) with only one option, don't show a
            // menu for this level, but keep its info in addDsMenus for trackPath sanity.
            hidden = (! atLeaf && objList.size === 1);
            mutState.setIn(['addDsInfo', 'menus'],
                           addDsMenus.splice(ix, addDsMenus.size,
                                             this.makeMenu(objList, selectedObj, menuLabel,
                                                           valueField, optionLabelFunc, hidden)));
        }
        return selectedObj;
    },

    rMakeSubtrackMenus: function(mutState, trackObjList, changedIx, trackPath, ix) {
        // Recursively build subtrack (or view) menus (when the selected track has subtracks).
        // Use shortLabel for menu labels.
        var selectedTrackObj = this.updateAddDsMenu(mutState, changedIx, ix,
                                                    trackObjList, trackPath.get(ix),
                                                    'subtrack', 'track', this.trackLabel,
                                                    'subtracks');
        var subtrackObjList = selectedTrackObj.get('subtracks');
        if (subtrackObjList) {
            this.rMakeSubtrackMenus(mutState, subtrackObjList, changedIx, trackPath, ix+1);
        }
    },

    updateAddDsSchemaUrl: function(mutState, db) {
        // Read the selected path back out of addDsInfo.menus and update addDsInfo.schemaUrl.
        var trackPath = this.getAddDsTrackPath(mutState);
        mutState.setIn(['addDsInfo', 'schemaUrl'],
                       this.schemaUrlFromTrackPath(mutState, db, trackPath));
    },

    disableDataSourcesInAddDsMenus: function (mutState) {
        // Disable the leaf (rightmost) menu's options only for already-added data sources.
        var dataSources = mutState.get('dataSources');
        if (! dataSources) {
            return;
        }
        var addDsMenusPath = ['addDsInfo', 'menus'];
        var addDsMenus = mutState.getIn(addDsMenusPath);
        var leafIx = addDsMenus.size - 1;
        var leafOptionsPath = addDsMenusPath.concat([leafIx, 'valLabels']);
        // Remove all menu options' disabled flags:
        addDsMenus.forEach(function(menuInfo, menuIx) {
            menuInfo.get('valLabels').forEach(function(valLabel, optionIx) {
                var fullPath = addDsMenusPath.concat([menuIx, 'valLabels', optionIx, 'disabled']);
                mutState.removeIn(fullPath);
            });
        });
        // Disable any leaf menu option that matches any data source path:
        dataSources.forEach(function(ds) {
            var trackPath = ds.get('trackPath').toJS();
            if (trackPath.length !== addDsMenus.size) {
                // Leaves can't match, so skip this one:
                return true;
            }
            _.forEach(trackPath, function(tpValue, tpIx) {
                if (tpIx === leafIx) {
                    mutState.getIn(leafOptionsPath).forEach(function(option, opIx) {
                        var optionDisablePath;
                        if (option.get('value') === tpValue) {
                            optionDisablePath = leafOptionsPath.concat([opIx, 'disabled']);
                            mutState.setIn(optionDisablePath, 'true');
                            // All done -- break out of forEach
                            return false;
                        }
                        return true;
                    });
                } else if (tpValue !== addDsMenus.getIn([tpIx, 'selected'])) {
                    // Mismatch at non-leaf level -- break out of _.forEach
                    return false;
                }
            });
        });
    },

    disableNoGenomeInAddDsMenus: function (mutState) {
        // If region is genome, then disable each Add Data Sources menu option with noGenome flag.
        // If caller is also calling disableDataSourcesInAddDsMenus, call that first because it
        // removes pre-existing disabled flags!
        var menusPath, addDsMenus;
        if (this.regionIsGenome(mutState)) {
            menusPath = ['addDsInfo', 'menus'];
            addDsMenus = mutState.getIn(menusPath);
            addDsMenus.forEach(function (menu, menuIx) {
                var options = menu.get('valLabels');
                options.forEach(function(option, optionIx) {
                    var fullPath;
                    if (option.get('noGenome')) {
                        fullPath = menusPath.concat([menuIx, 'valLabels', optionIx, 'disabled']);
                        mutState.setIn(fullPath, 'true');
                    }
                });
            });
        }
    },

    addDsSelectedIsDisabled: function (mutState) {
        // Follow addDsTrackPath through the menu options; if any option is disabled, return true.
        var trackPath = this.getAddDsTrackPath(mutState) || Immutable.List();
        var trackPathIsDisabled = false;
        trackPath.forEach(function(tpValue, tpIx) {
            var options = mutState.getIn(['addDsInfo', 'menus', tpIx, 'valLabels']) ||
                          Immutable.List();
            var selected = options.find(function(opt) { return (opt.get('value') === tpValue); });
            if (selected && selected.get('disabled')) {
                trackPathIsDisabled = true;
                // All done, break out of forEach:
                return false;
            }
        });
        return trackPathIsDisabled;
    },

    updateAddDsButton: function(mutState) {
        // If user has added the max # of data sources, or has already added the currently
        // selected track, disable the Add button.  Call this after we have determined which
        // menu options are disabled.
        var dataSources = mutState.get('dataSources');
        if (! dataSources) {
            return;
        }
        var groupMenuOptions = mutState.getIn(['addDsInfo', 'menus', 0, 'valLabels']);
        var emptyMenus = !groupMenuOptions || groupMenuOptions.size < 1;
        var disabled = (dataSources.size >= this.maxDataSources ||
                        this.addDsSelectedIsDisabled(mutState) ||
                        emptyMenus);
        mutState.setIn(['addDsInfo', 'disabled'], disabled);
    },

    updateAddDsSection: function (mutState) {
        // Whenever there is a change to dataSources, to a menu in the Add Data Sources section,
        // or to region, update several dependencies in the Add Data Sources section.
        this.checkNoGenome(mutState);
        // These functions must be called in the right order:
        this.disableDataSourcesInAddDsMenus(mutState);
        this.disableNoGenomeInAddDsMenus(mutState);
        this.updateAddDsButton(mutState);
    },

    groupedTrackDbToMenus: function (mutState, db, trackPath, changedIx) {
        // Build or update the list of menu descriptors for rendering group, track, and possibly
        // view and subtrack menus for choosing a data source.  Use trackPath as a record of
        // selected items, if it's consistent with groupedTrackDb.
        // If changedIx is given and trackPath[<changedIx] is consistent with addDsMenus,
        // rebuild only from changedIx on down, otherwise completely rebuild.
        var grpTrackDb = mutState.get('groupedTrackDb');
        var selectedGroupObj, trackObjList, ix;
        changedIx = changedIx || 0;
        // The first menu is for track groups:
        ix = 0;
        selectedGroupObj = this.updateAddDsMenu(mutState, changedIx, ix,
                                                grpTrackDb, trackPath.get(ix),
                                                'track group', 'name', this.groupLabel, 'tracks');
        // The second menu is for tracks or composites in the selected group; if there
        // are views and/or subtracks, recursively descend to those.
        if (selectedGroupObj) {
            ix = 1;
            trackObjList = selectedGroupObj.get('tracks');
            this.rMakeSubtrackMenus(mutState, trackObjList, changedIx, trackPath, ix);
        }
        // Now update the things that depend on menus and other state:
        this.updateAddDsSection(mutState);
        this.updateAddDsSchemaUrl(mutState, db);
    },

    handleGroupedTrackDb: function(mutState, cartVar, newValue) {
        // Server just sent new groupedTrackDb; if it is for the current db,
        // build group/track/etc menus for <AddDataSource>.
        var currentDb, addDsTrackPath;
        if (cartVar === 'groupedTrackDb') {
            // The groupedTrackDb from cartJson is a wrapper object with children db
            // and groupedTrackDb.  If db matches current db, store the inner groupedTrackDb.
            // For GenArk/hub assemblies, the server transforms db (e.g. GCA_002077035.3) to a
            // hub-decorated name (e.g. hub_123_GCA_002077035.3), so also check if the
            // server's db contains the client's db.
            currentDb = this.getDb(mutState);
            if (! currentDb || newValue.db === currentDb ||
                (newValue.db && newValue.db.indexOf(currentDb) >= 0)) {
                // Remove track groups with no tracks
                _.remove(newValue.groupedTrackDb, function(trackGroup) {
                    return ! (trackGroup.tracks && trackGroup.tracks.length > 0);
                });
                mutState.set('groupedTrackDb', Immutable.fromJS(newValue.groupedTrackDb));
                // If the cart includes a saved trackPath consistent with groupedTrackDb, use that:
                addDsTrackPath = mutState.get(this.addDsSelectVar);
                if (! addDsTrackPath ||
                    ! this.isInGroupedTrackDb(mutState, addDsTrackPath)) {
                    addDsTrackPath = Immutable.List();
                }
                this.groupedTrackDbToMenus(mutState, newValue.db, addDsTrackPath, 0);
            }
        } else {
            this.error('handleGroupedTrackDb: expecting cartVar groupedTrackDb, got ',
                       cartVar, newValue);
        }
    },

    handleJsonBlob: function(mutState, cartVar, newValue) {
        // Parse JSON cartVar and store as Immutable state -- or remove if set to empty.
        if (newValue) {
            mutState.set(cartVar, Immutable.fromJS(JSON.parse(newValue)));
        } else {
            mutState.remove(cartVar);
        }
    },

    labelFromTrackPath: function(mutState, trackPath) {
        // Return a text label for the dataSource identified by trackPath
        var trackLeafObj = this.findGroupedTrack(mutState, trackPath);
        var compositeObj, label;
        if (trackLeafObj) {
            if (trackPath.size > 2) {
                // Get the composite object too, using a truncated trackPath:
                compositeObj = this.findGroupedTrack(mutState,
                                                     trackPath.splice(2, trackPath.size));
                label = compositeObj.get('shortLabel') + ' - ' + trackLeafObj.get('shortLabel');
            } else {
                label = trackLeafObj.get('shortLabel');
            }
        } else {
            this.error('labelFromTrackPath: can\'t find track for trackPath ' +
                       trackPath.toString(), trackPath);
            label = trackPath.last();
        }
        return label;
    },

    validateCart: function(mutState) {
        // When both groupedTrackDb and hgi_querySpec have been incorporated into mutState,
        // trim any data sources from hgi_querySpec that aren't found in groupedTrackDb
        // (e.g. after a db change).
        // Also make sure that labels and schema links are consistent w/latest db & trackDb.
        var dataSources = mutState.get('dataSources') || Immutable.List();
        var addDsInfo = mutState.get('addDsInfo');
        if (dataSources && addDsInfo) {
            //#*** Seems awfully inefficient to do this every time something comes in from
            //#*** the server.  It would be better to have a coherent way to manage multiple
            //#*** arriving pieces of data together.
            var db = this.getDb(mutState);
            var newDataSources = dataSources.filter(function(ds) {
                var trackPath = ds.get('trackPath');
                return trackPath && this.isInGroupedTrackDb(mutState, trackPath);
            }, this
                                                    ).map(function(ds) {
                var trackPath = ds.get('trackPath');
                ds = ds.set('label', this.labelFromTrackPath(mutState, trackPath));
                ds = ds.set('schemaUrl', this.schemaUrlFromTrackPath(mutState, db, trackPath));
                return ds;
            }, this);
            if (! newDataSources.equals(dataSources)) {
                mutState.set('dataSources', newDataSources);
            }
            this.updateAddDsSection(mutState);
        }
    },

    changeRange: function(mutState, uiPath, newValue) {
        // User changed 'region to annotate' select; if they changed it to 'define regions'
        // but have not yet defined any regions, open the UserRegions popup.
        //#*** Reaching into a mixin's state here is not good.
        var existingRegions = mutState.getIn(['regionSelect', 'userRegions', 'regions']);
        if (newValue === 'userRegions' && (!existingRegions || existingRegions === "")) {
            this.openUserRegions(mutState);
        } else {
            this.changeCartString(mutState, uiPath, newValue);
        }
        // Depending on whether region is genome, some tracks may be disabled (or not).
        this.updateAddDsSection(mutState);
    },

    clearUserRegions: function(mutState) {
        // Clear user region state and reset rangeSelectVar.
        this.changeCartString(mutState, ['regionSelect', this.rangeSelectVar], 'position');
        this.clearUserRegionState(mutState);
    },

    uploadedUserRegions: function(mutState) {
        // Wait for results of parsing uploaded region file
        mutState.setIn(['regionSelect', 'loading'], true);
    },

    pastedUserRegions: function(mutState) {
        // Update rangeSelectVar to select user regions
        mutState.setIn(['regionSelect', this.rangeSelectVar], 'userRegions');
    },

    toQuerySpecJS: function(mutState) {
        // Transform internal state into the JS object version of the query specification
        // that the CGI will receive as this.querySpecVar.
        var querySpec = {};
        var dataSources = mutState.get('dataSources').toJS();
        // Include only the parts of dataSources that are relevant to the query, not UI decorations:
        querySpec.dataSources = _.map(dataSources, function(ds) {
            return _.pick(ds, ['trackPath']);
        });
        querySpec.outFileOptions = mutState.get('outFileOptions').toJS();
        // tableFields for current dataSources will be added in below so delete the current+old:
        delete querySpec.outFileOptions.tableFields;
        // Add in uiChoices that apply to dataSources and outFileOptions.
        _.forEach(dataSources, function(ds, dsIx) {
            var track = _.last(ds.trackPath);
            // Build a relatedTables data structure for this track: an ordered list of objects
            // containing table & ordered list of enabled fields.
            // First get list of selected related tables, if any:
            var relSel = this.getUiChoice(mutState, ['relatedSelected', track]);
            var tfChoicesMain = this.getUiChoice(mutState, ['tableFields', track]) ||
                               Immutable.Map();
            var tfChoicesRel = this.getUiChoice(mutState, ['relatedTableFields', track]) ||
                               Immutable.List();
            var relatedTables, tfChoices;
            if (relSel && relSel.size > 0) {
                // Make the relatedTables data structure with empty field lists:
                relatedTables = relSel.map(
                    function(table) {
                        return { table: table, fields: [] };
                    },
                    this).toArray();
                // Now add the selected fields -- from only the selected related tables:
                tfChoices = tfChoicesRel.filter(function (fields, table) {
                    return relSel.find(function(t) { return t === table; });
                });
                if (tfChoices && tfChoices.size > 0) {
                    _.forEach(relatedTables,
                        function (info) {
                            var fields = tfChoices.get(info.table);
                            if (fields) {
                                info.fields = fields.toArray();
                            }
                        }
                    );
                }
                _.set(querySpec, ['dataSources', dsIx, 'config', 'relatedTables'],
                      relatedTables);
                // n/a for missing values, to match hgTables
                _.set(querySpec, ['dataSources', dsIx, 'config', 'naForMissing'], true);
            }
            if (tfChoicesMain) {
                _.set(querySpec, ['outFileOptions', 'tableFields', track],
                      tfChoicesMain.toJS());
            }
        }, this);
        console.log('new querySpec:', querySpec);
        return querySpec;
    },

    cartSendQuerySpec: function(mutState) {
        // When some part of querySpec changes, update the cart variable.
        var querySpec = this.toQuerySpecJS(mutState);
        this.cartSet(this.querySpecVar, querySpec);
    },

    changeAddDsMenu: function(mutState, uiPath, newValue) {
        // User changed group, track, view, or subtrack in the 'Add Data Source' section.
        // Changing group or track (or view) has side effects on lower-level menus.
        var ix = uiPath.pop();
        var trackPath = this.getAddDsTrackPath(mutState);
        var db = this.getDb(mutState);
        if (trackPath.size === 0) {
            this.error('changeAddDsMenu: getAddDsTrackPath came up empty?! ix=' + ix);
            ix = 0;
        } else if (ix >= trackPath.size) {
            this.error('changeAddDsMenu: trackPath size is ' + trackPath.size +
                       ' but got ix ' + ix, trackPath, ix);
            // Rebuild menus from scratch
            trackPath = Immutable.List();
            ix = 0;
        } else {
            // Normal case: plop newValue into trackPath[ix], truncating anything after ix:
            trackPath = trackPath.splice(ix, trackPath.size, newValue);
        }
        // Regenerate menus
        this.groupedTrackDbToMenus(mutState, db, trackPath, ix);
        // Store updated trackPath in state and cart
        trackPath = this.getAddDsTrackPath(mutState);
        mutState.set(this.addDsSelectVar, trackPath);
        this.cartSet(this.addDsSelectVar, trackPath);
    },

    addDataSource: function(mutState) {
        // User clicked button to add a new data source (track/table); tell server and render,
        // unless they have reached the max number of allowed data sources, or are trying to
        // add a data source that has already been added.
        if (mutState.get('dataSources').size >= this.maxDataSources) {
            alert("Sorry, you can choose a maximum of " + this.maxDataSources + " data sources.");
            return;
        }
        var addDsTrackPath = this.getAddDsTrackPath(mutState);
        var label = this.labelFromTrackPath(mutState, addDsTrackPath);
        var alreadyAdded = false;
        mutState.get('dataSources').forEach(function (ds) {
            var dsTrackPath = ds.get('trackPath');
            if (dsTrackPath.equals(addDsTrackPath)) {
                alreadyAdded = true;
                return false;
            }
        });
        if (alreadyAdded) {
            alert(label + ' has already been added above.');
            return;
        }
        var db = this.getDb(mutState);
        var schemaUrl = this.schemaUrlFromTrackPath(mutState, db, addDsTrackPath);
        var dataSource = Immutable.fromJS({ trackPath: addDsTrackPath,
                                            label: label,
                                            schemaUrl: schemaUrl});
        mutState.update('dataSources', function(list) {
            return list.push(dataSource);
        });
        this.updateAddDsSection(mutState);
        this.cartSendQuerySpec(mutState);
    },

    goToCgi: function(cgiName) {
        // Send user to cgi by setting jumpForm's action and submitting
        var $form = $('#jumpForm');
        $form.attr("action", cgiName);
        $form[0].submit();
    },

    goToTrackHubs: function() {
        this.goToCgi("hgHubConnect");
    },

    goToCustomTracks: function() {
        this.goToCgi("hgCustom");
    },

    reorderDataSources: function(mutState, path, reordering) {
        // User dragged and dropped a Data Source section to a new position; update accordingly.
        // reordering is an array whose indices are new positions and values are old positions.
        mutState.update('dataSources', function(dataSources) {
            if (reordering.length !== dataSources.size) {
                console.warn(dataSources, reordering);
                this.error('reorderDataSources: there are', dataSources.size, 'data sources but',
                           reordering.length, 'elements in the reordering array from Sortable.');
            }
            var i, oldPos, newDataSources = [];
            for (i = 0;  i < dataSources.size;  i++) {
                oldPos = reordering[i];
                newDataSources[i] = dataSources.get(oldPos);
            }
            return Immutable.List(newDataSources);
        });
        this.cartSendQuerySpec(mutState);
    },

    dataSourceClick: function(mutState, path, data) {
        // User clicked on something in a DataSource
        if (path[1] === 'reorder') {
            this.reorderDataSources(mutState, path, data);
        } else if (path.length < 2) {
            this.error('appModel.click: path has dataSources but not ix:', path);
        } else {
            var ix = path[1], action = path[2];
            if (action && action === 'remove') {
                mutState.update('dataSources', function(oldList) {
                    return oldList.remove(ix);
                });
                this.updateAddDsSection(mutState);
                this.cartSendQuerySpec(mutState);
            } else {
                this.error('dataSourceClick: unrecognized path', path);
            }
        }
    },

    getTableStringForTrack: function(mutState, track) {
        var tableStr = track;
        // Add in related tables, if any
        var relSel = this.getUiChoice(mutState, ['relatedSelected', track]);
        if (relSel) {
            relSel.forEach(function(relTable) {
                tableStr += ',' + relTable;
            });
        }
        return tableStr;
    },

    doChooseFields: function(mutState) {
        // User clicked the 'Choose fields' button -- get field info from server.
        var dataSources = mutState.get('dataSources');
        var tableGroupStr = dataSources.map(function(dataSource) {
            var track = dataSource.get('trackPath').last();
            return this.getTableStringForTrack(mutState, track);
        }, this).toJS().join(';');
        // Create empty fieldSelect state so the dialog will pop up and show a spinner.
        // Sometimes these requests can take a while.
        mutState.set('fieldSelect', Immutable.Map());
        this.cartDo({ getFields: { tableGroups: tableGroupStr, jsonName: 'fieldSelect' },
                      getRelatedTables: { tableGroups: tableGroupStr,
                                          jsonName: 'relatedTablesAll' } });
    },

    fieldSelectRemove: function(mutState) {
        // Make the 'Choose Fields' popup go away
        mutState.set('fieldSelect', null);
    },

    fieldSelectToUiChoices: function(mutState, track, table, fsPathToFields) {
        // Regenerate the uiChoices for track/table/fields (after a field has been changed).
        // fieldList (fieldSelect.track.tableFields.table.fields) is an OrderedMap, and it
        // is ordered as the columns in the table, not in the order that the checkboxes are
        // checked by the user.  Preserve the order by generating a new ordered map for
        // main table fields, or a new list for related table fields.
        var fieldList = mutState.getIn(fsPathToFields);
        var nonDefaultFields = fieldList.filter(
                                   function(fieldInfo) {
                                       var field = fieldInfo.get('name');
                                       return (fieldInfo.get('checked') !==
                                               this.fieldCheckedByDefault(table, field));
                                   }, this);
        var newOMap, newList;
        if (table === track) {
            newOMap = Immutable.OrderedMap(
                nonDefaultFields.map(
                    function(fieldInfo) {
                        return [fieldInfo.get('name'), fieldInfo.get('checked')];
                    })
            );
            this.setUiChoice(mutState, ['tableFields', track], newOMap);
        } else {
            newList = nonDefaultFields.map(function(fieldInfo) { return fieldInfo.get('name'); });
            this.setUiChoice(mutState, ['relatedTableFields', track, table], newList);
        }
    },

    fieldSelectSetOne: function(mutState, path, newValue) {
        // Set (or clear) the checkbox for a particular field
        var track = path[3];
        var table = path[4];
        var field = path[5];
        var fsPathToFields = ['fieldSelect', track, 'tableFields', table, 'fields'];
        var fieldList = mutState.getIn(fsPathToFields);
        var fieldIx = fieldList.findIndex(
            function (fieldInfo) {
                return fieldInfo.get('name') === field;
            });
        if (fieldIx < 0) {
            this.error('fieldSelectSetOne: unrecognized field "' + field + '" for track ' +
                       track + ', table ' + table);
            return;
        }
        // Update field's status in popup, uiChoices and cart
        //#*** TODO: figure out a way to avoid duplication of state in popup and uiChoices.
        //#*** Reconstructing popup all the time is more functional but loses Immutable benefits.
        //#*** Perhaps, when popup is open, it owns the tableFields state, and feeds that
        //#*** back into uiChoices when the user clicks Done.  That also
        //#*** would work better if we want to make popup actions cancellable.
        //#*** Or, if we can do something like make the popup state a bunch of cursors into
        //#*** uiChoices, that might solve the bogus duplication problem.
        mutState.setIn(fsPathToFields.concat([fieldIx, 'checked']), newValue);
        this.fieldSelectToUiChoices(mutState, track, table, fsPathToFields);
        this.cartUpdateUiChoices(mutState, this.uiChoicesVar);
        this.cartSendQuerySpec(mutState);
    },

    fieldSelectSetAll: function(mutState, path, newValue) {
        // 'Set all' or 'Clear all' according to newValue
        var track = path[3];
        var table = path[4];
        var fsPathToFields = ['fieldSelect', track, 'tableFields', table, 'fields'];
        // Update all of table's fields in popup, uiChoices and cart.
        //#*** as above, reduce state duplication
        mutState.updateIn(fsPathToFields,
                          function(fieldVals) {
                              return fieldVals.map(
                                  function(fieldInfo) {
                                      return fieldInfo.set('checked', newValue);
                                  });
                          });
        this.fieldSelectToUiChoices(mutState, track, table, fsPathToFields);
        this.cartUpdateUiChoices(mutState, this.uiChoicesVar);
        this.cartSendQuerySpec(mutState);
    },

    fieldSelectSelectRelated: function(mutState, path, newValue) {
        // User changed the select input for choosing a related table
        var track = path[3];
        mutState.setIn(['fieldSelect', track, 'relatedAvailable', 'selected'], newValue);
        this.setUiChoice(mutState, ['relatedAvailable', track, 'selected'], newValue);
        // Update menus of related tables and the flag to disable the add button.
        this.updateRelatedTablesMenus(mutState);
        this.cartUpdateUiChoices(mutState, this.uiChoicesVar);
    },

    fieldSelectAddRelated: function(mutState, path) {
        // User wants to add the selected related table for track
        var track = path[3];
        var relTable = mutState.getIn(['fieldSelect', track, 'relatedAvailable', 'selected']);
        var relSel = this.getUiChoice(mutState, ['relatedSelected', track]) || Immutable.List();
        var alreadyAddedTable = relSel.some(function (t) { return t === relTable; });
        var currentTables = mutState.getIn(['fieldSelect', track, 'tableFields']);
        var relTableCount = currentTables.size - 1;
        var newRelSel, tableGroupStr, label;
        // Watch out for duplicate adds or trying to add more than the max.
        if (alreadyAddedTable) {
            // Prepend db to related tables that start with ".".
            label = relTable;
            if (relTable.charAt(0) === ".") {
                label = this.getDb(mutState) + relTable;
            }
            alert("Table " + label + " has already been added.");
            return;
        }
        if (relTableCount >= this.maxRelatedTables) {
            alert("Sorry, you can choose a maximum of " + this.maxRelatedTables +
                  " related tables.");
            return;
        }
        if (relTable) {
            // Add a stub relTable section (with spinner until server response gives fields)
            label = mutState.getIn(['fieldSelect', track, 'relatedAvailable', 'options'])
                            .find(
                function (option) {
                    return option.get('value') === relTable;
                }
            ).get('label');
            mutState.setIn(['fieldSelect', track, 'tableFields', relTable, 'label'],
                           label);
            // Add relTable to the list of selected related tables for this dataSource
            newRelSel = relSel.push(relTable);
            this.setUiChoice(mutState, ['relatedSelected', track], newRelSel);
            // Update menus of related tables to gray out the newly selected one.
            this.updateRelatedTablesMenus(mutState);
            this.cartUpdateUiChoices(mutState, this.uiChoicesVar);
            this.cartSendQuerySpec(mutState);
            tableGroupStr = this.getTableStringForTrack(mutState, track);
            this.cartDo({ getFields: { tableGroups: tableGroupStr,
                                       jsonName: 'relatedFields' },
                          getRelatedTables: { tableGroups: tableGroupStr,
                                              jsonName: 'relatedTablesUpdate' }
                        });
        } else {
            this.error('Got "addRelated" event but can\'t find selected related table');
        }
    },

    fieldSelectRemoveRelated: function(mutState, path) {
        // User doesn't want to choose fields from this related table after all.
        var track = path[3];
        var relTable = path[4];
        var tableGroupStr;
        // Remove relTable from the list of selected related tables for this dataSource
        var relSel = this.getUiChoice(mutState, ['relatedSelected', track]) || Immutable.List();
        var newRelSel = relSel.filter(function(table) { return table !== relTable; });
        this.setUiChoice(mutState, ['relatedSelected', track], newRelSel);
        // Remove relTable's section from fieldSelect Modal
        mutState.removeIn(['fieldSelect', track, 'tableFields', relTable]);
        // Update menus of related tables to show that relTable is now available for selection.
        this.updateRelatedTablesMenus(mutState);
        this.cartUpdateUiChoices(mutState, this.uiChoicesVar);
        this.cartSendQuerySpec(mutState);
        // Get the possibly expanded list of related tables from the server.
        tableGroupStr = this.getTableStringForTrack(mutState, track);
        this.cartDo({ getRelatedTables: { tableGroups: tableGroupStr,
                                          jsonName: 'relatedTablesUpdate' } });
    },

    doGetOutput: function(mutState) {
        // User clicked 'Get output' button; make a form and submit it.
        var querySpec = this.toQuerySpecJS(mutState), querySpecStr;
        var doFile = mutState.getIn(['outFileOptions', 'doFile']);
        if (querySpec.dataSources.length < 1) {
            alert('Please add at least one data source.');
        } else {
            if (! doFile) {
                // Show loading image and message that the query might take a while.
                mutState.set('showLoadingImage', true);
            }
            querySpecStr = encodeURIComponent(JSON.stringify(querySpec));
            $('input[name="' + this.querySpecVar + '"]').val(querySpecStr);
            $('#queryForm')[0].submit();
        }
    },

    outFileOptionsClick: function(mutState, path, newValue) {
        // User clicked on something in OutFileOptions; path[1] is either an action or
        // the piece of state that needs to be updated on server
        if (! _.includes(['chooseFields', 'fieldSelect', 'getOutput'], path[1])) {
            mutState.setIn(path, newValue);
            this.cartSendQuerySpec(mutState);
        }
    },

    onChangeDb: function(mutState) {
        // The CladeOrgDbPos mixin handles the clade/org/db menus, position, and
        // notifies the server about the change.  However, for databases that have
        // giant trackDbs, it can take a long time before that info shows up -- long
        // enough for the user to start playing with Add Data Source menus if they
        // are left in place.  So, clear out db-specific state until the new data arrive.
        mutState.remove('groupedTrackDb');
        mutState.remove('addDsInfo');
        // Update PositionSearchMixin's position:
        var newPos = this.getDefaultPos(mutState);
        this.setPosition(mutState, newPos);
        // Parallel requests for little stuff that we need ASAP and potentially huge trackDb:
        this.cartDo({ get: { 'var': 'position' },
                      getQueryState: {},
                      getGeneSuggestTrack: {},
                      getUserRegions: {}
                      });
        this.cartDo({ getGroupedTrackDb: { fields: this.tdbFields,
                                           excludeTypes: this.excludeTypes } });
    },

    initialize: function() {
        // Register handlers for cart variables that need special treatment:
        this.registerCartVarHandler(['userRegionsUpdate', 'userRegions'],
                                    this.handleUserRegionsUpdate);
        this.registerCartVarHandler('queryState', this.handleQueryState);
        this.registerCartVarHandler(this.rangeSelectVar, this.handleRangeSelect);
        this.registerCartVarHandler('fieldSelect', this.handleFieldSelect);
        this.registerCartVarHandler('relatedFields', this.handleRelatedFields);
        this.registerCartVarHandler(['relatedTablesAll', 'relatedTablesUpdate'],
                                    this.handleRelatedTables);
        this.registerCartVarHandler(this.addDsSelectVar, this.handleJsonBlob);
        this.registerCartVarHandler('groupedTrackDb', this.handleGroupedTrackDb);
        this.registerCartValidateHandler(this.validateCart);
        // Register handlers for UI events:
        this.registerUiHandler(['regionSelect', this.rangeSelectVar], this.changeRange);
        this.registerUiHandler(['regionSelect', 'changeRegions'], this.openUserRegions);
        this.registerUiHandler(['regionSelect', 'clearRegions'], this.clearUserRegions);
        this.registerUiHandler(['regionSelect', 'userRegions', 'uploaded'],
                               this.uploadedUserRegions);
        this.registerUiHandler(['regionSelect', 'userRegions', 'pasted'], this.pastedUserRegions);
        this.registerUiHandler(['addDsMenuSelect'], this.changeAddDsMenu);
        this.registerUiHandler(['addDataSource'], this.addDataSource);
        this.registerUiHandler(['trackHubs'], this.goToTrackHubs);
        this.registerUiHandler(['customTracks'], this.goToCustomTracks);
        this.registerUiHandler(['dataSources'], this.dataSourceClick);
        this.registerUiHandler(['outFileOptions'], this.outFileOptionsClick);
        this.registerUiHandler(['outFileOptions', 'chooseFields'], this.doChooseFields);
        this.registerUiHandler(['outFileOptions', 'fieldSelect', 'remove'],
                               this.fieldSelectRemove);
        this.registerUiHandler(['outFileOptions', 'fieldSelect', 'checked'],
                               this.fieldSelectSetOne);
        this.registerUiHandler(['outFileOptions', 'fieldSelect', 'setAll'],
                               this.fieldSelectSetAll);
        this.registerUiHandler(['outFileOptions', 'fieldSelect', 'selectRelated'],
                               this.fieldSelectSelectRelated);
        this.registerUiHandler(['outFileOptions', 'fieldSelect', 'addRelated'],
                               this.fieldSelectAddRelated);
        this.registerUiHandler(['outFileOptions', 'fieldSelect', 'removeRelated'],
                               this.fieldSelectRemoveRelated);
        this.registerUiHandler(['outFileOptions', 'getOutput'], this.doGetOutput);

        // Tell cart what CGI to send requests to
        this.cart.setCgi('hgIntegrator');

        // Bootstrap initial state from page or by server request, then render:
        if (window.initialAppState) {
            this.state = Immutable.fromJS(window.initialAppState);
        } else {
            this.state = Immutable.Map();
            // Fire off some requests in parallel -- some are much slower than others.
            // Get clade/org/db and important small cart variables.
            this.cartDo({ getCladeOrgDbPos: {},
                          getQueryState: {},
                          get: {'var': this.rangeSelectVar + ',' + this.addDsSelectVar},
                          getUserRegions: {}
                        });
            // The groupedTrackDb structure is so large for hg19 and other well-annotated
            // genomes that the volume of compressed JSON takes a long time on the wire.
            this.cartDo({ getGroupedTrackDb: { fields: this.tdbFields,
                                               excludeTypes: this.excludeTypes }
                        });
        }
        this.cart.flush();
        this.render();
    }


}); // HgIntegratorModel

var container = document.getElementById('appContainer');

function render(state, update, undo, redo) {
    // ImModel calls this function after making changes to state.
    // Pass the immutable state object and callbacks to our top-level react component:
    React.render(React.createElement(AppComponent, {appState: state,
                                                    update: update, undo: undo, redo: redo }),
		 container);
}

var hgIntegratorModel = new HgIntegratorModel(render);

hgIntegratorModel.initialize();
