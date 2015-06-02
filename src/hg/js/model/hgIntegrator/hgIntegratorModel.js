/* global AppComponent, ImModel, CladeOrgDbMixin, PositionSearchMixin, UserRegionsMixin */

var HgIntegratorModel = ImModel.extend({
    // Handle hgIntegrator's UI state and events and interaction with the server/CGI/cart.

    mixins: [ CladeOrgDbMixin(['cladeOrgDb']),
              PositionSearchMixin(['regionSelect', 'positionInfo']),
              UserRegionsMixin(['regionSelect', 'userRegions'])],

    maxDataSources: 5,
    tdbFields: 'track,table,shortLabel,parent,subtracks',
    excludeTypes: 'bam,wigMaf',

    handleCartVar: function(mutState, cartVar, newValue) {
        // Some cart variables require special action (not simply being merged into top-level state)
        if (cartVar === 'hgi_querySpec') {
            if (newValue === '') {
                // No querySpec in cart yet -- make an empty one for rendering:
                mutState.set(cartVar,
                                  Immutable.fromJS({ dataSources: [], outFileOptions: {} }));
            } else {
                newValue = decodeURIComponent(newValue);
                newValue = JSON.parse(newValue);
                mutState.set(cartVar, Immutable.fromJS(newValue));
            }
        } else if (cartVar === 'hgi_range') {
            // perhaps regionSelect stuff should move to a plugin
            mutState.setIn(['regionSelect', cartVar], newValue);
            mutState.setIn(['regionSelect', 'loading'], false);
        } else if (cartVar === 'userRegionsUpdate') {
            if (newValue && newValue !== "") {
                // User-defined regions updated -- change range to userRegions
                mutState.setIn(['regionSelect', 'hgi_range'], 'userRegions');
            }
        } else if (cartVar === 'tableFields') {
            // The server has sent an object mapping table names to lists of fields.
            // Construct an immutable map of table names to ordered maps of fields to
            // booleans for checkboxes; if hgi_querySpec contains saved settings,
            // use those; otherwise true unless field is bin.
            var tfDefaults = {};
            var current = mutState.getIn(['hgi_querySpec', 'outFileOptions', 'tableFields']);
            if (current) {
                current = current.toJS();
            } else {
                current = {};
            }
            tfDefaults = _.mapValues(newValue, function(info, table) {
                var currentFieldSettings = current[table] || {};
                var newFieldSettings = Immutable.OrderedMap();
                info.fields.forEach(function(fieldAndDesc) {
                    var field = fieldAndDesc.name;
                    var checked = currentFieldSettings[field];
                    if (checked === undefined) {
                        checked = (field !== 'bin');
                    }
                    var newSetting = Immutable.Map({checked: checked, desc: fieldAndDesc.desc});
                    newFieldSettings = newFieldSettings.set(field, newSetting);
                }, this);
                return Immutable.Map({ fields: newFieldSettings, label: info.label});
            }, this);
            mutState.set('tableFields', Immutable.OrderedMap(tfDefaults));
        } else {
            this.error('handleCartVar: unexpectedly given', cartVar, newValue);
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

    schemaUrlFromTrackPath: function(mutState, trackPath) {
        // Formulate a link to hgTables' table schema for trackPath's group/track/table.
        var db = mutState.getIn(['cladeOrgDb', 'db']);
        var group = trackPath.first();
        var track = trackPath.get(1);
        var table = trackPath.last();
        return 'hgTables?db=' + db + '&hgta_group=' + group + '&hgta_track=' + track +
                        '&hgta_table=' + table + '&hgta_doSchema=1';
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

    makeValLabel: function(value, label) {
        // Return a {value, label} object suitable for describing <LabeledSelect> options
        return Immutable.Map({ value: value, label: label });
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

    makeMenu: function(objList, selectedObj, label, valueField, labelFunc, hidden) {
        // Return an Immutable menu descriptor object suitable for <LabeledSelectRow>.
        // objList must be an Immutable.List of Immutable objects that have valueField.
        // labelFunc, when called on a member of objList, returns a string.
        // selectedObj must be a member of objList or falsey.
        var valLabels = objList.map(function(obj) {
            return this.makeValLabel(obj.get(valueField), labelFunc(obj));
        }, this);
        var selected = selectedObj ? selectedObj.get(valueField) : null;
        return Immutable.fromJS({ valLabels: valLabels,
                                  selected: selected,
                                  label: label,
                                  hide: hidden });
    },

    updateAddDsMenu: function(mutState, changedIx, ix, objList, selectedValue,
                              menuLabel, valueField, labelFunc, childField) {
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
                                                           valueField, labelFunc, hidden)));
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

    updateAddDsSchemaUrl: function(mutState) {
        // Read the selected path back out of addDsInfo.menus and update addDsInfo.schemaUrl.
        var trackPath = this.getAddDsTrackPath(mutState);
        var leafObj = this.findGroupedTrack(mutState, trackPath);
        if (leafObj) {
            trackPath = trackPath.set(trackPath.size-1, leafObj.get('table'));
            mutState.setIn(['addDsInfo', 'schemaUrl'],
                           this.schemaUrlFromTrackPath(mutState, trackPath));
        } else {
            if (trackPath.size > 0 && trackPath.get(0)) {
                this.error('updateAddDsSchemaUrl: can\'t find trackPath ' + trackPath.toString());
            }
            mutState.setIn(['addDsInfo', 'schemaUrl'], null);
        }
    },

    updateAddDsDisable: function(mutState) {
        // If user has added the max # of data sources, or has already added the currently
        // selected track, disable the Add button.
        var dataSources = mutState.getIn(['hgi_querySpec', 'dataSources']);
        if (! dataSources) {
            return;
        }
        var addDsTrackPath = this.getAddDsTrackPath(mutState);
        var alreadyAddedTrack = dataSources.some(function(ds) {
            var trackPath = ds.get('trackPath');
            return trackPath.equals(addDsTrackPath);
        });
        var groupMenuOptions = mutState.getIn(['addDsInfo', 'menus', 0, 'valLabels']);
        var emptyMenus = !groupMenuOptions || groupMenuOptions.size < 1;
        var disabled = (dataSources.size >= this.maxDataSources ||
                        alreadyAddedTrack ||
                        emptyMenus);
        mutState.setIn(['addDsInfo', 'disabled'], disabled);
    },

    disableDataSourcesInAddDsMenus: function (mutState) {
        // Disable the leaf (rightmost) menu's options only for already-added data sources.
        var dataSources = mutState.getIn(['hgi_querySpec', 'dataSources']);
        if (! dataSources) {
            return;
        }
        var addDsMenusPath = ['addDsInfo', 'menus'];
        var addDsMenus = mutState.getIn(addDsMenusPath);
        var leafIx = addDsMenus.size - 1;
        var leafOptionsPath = _.flatten([addDsMenusPath, leafIx, 'valLabels']);
        // Remove all menu options' disabled flags:
        addDsMenus.forEach(function(menuInfo, menuIx) {
            menuInfo.get('valLabels').forEach(function(valLabel, optionIx) {
                var fullPath = _.flatten([addDsMenusPath, menuIx, 'valLabels',
                                          optionIx, 'disabled']);
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
                            optionDisablePath = _.flatten([leafOptionsPath, opIx, 'disabled']);
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

    groupedTrackDbToMenus: function (mutState, trackPath, changedIx) {
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
        this.disableDataSourcesInAddDsMenus(mutState);
        this.updateAddDsDisable(mutState);
        this.updateAddDsSchemaUrl(mutState);
    },

    handleGroupedTrackDb: function(mutState, cartVar, newValue) {
        // Server just sent new groupedTrackDb; if it is for the current db,
        // build group/track/etc menus for <AddDataSource>.
        var currentDb, addDsTrackPath;
        if (cartVar === 'groupedTrackDb') {
            // The groupedTrackDb from cartJson is a wrapper object with children db
            // and groupedTrackDb.  If db matches current db, store the inner groupedTrackDb.
            currentDb = this.getDb(mutState);
            if (! currentDb || newValue.db === currentDb) {
                // Remove track groups with no tracks
                _.remove(newValue.groupedTrackDb, function(trackGroup) {
                    return ! (trackGroup.tracks && trackGroup.tracks.length > 0);
                });
                mutState.set('groupedTrackDb', Immutable.fromJS(newValue.groupedTrackDb));
                // If the cart includes a saved trackPath consistent with groupedTrackDb, use that:
                addDsTrackPath = mutState.get('hgi_addDsTrackPath');
                if (! addDsTrackPath ||
                    ! this.isInGroupedTrackDb(mutState, addDsTrackPath)) {
                    addDsTrackPath = Immutable.List();
                }
                this.groupedTrackDbToMenus(mutState, addDsTrackPath, 0);
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
        // When both groupedTrackDb and hgi_query have been incorporated into mutState,
        // trim any data sources from hgi_query that aren't found in groupedTrackDb
        // (e.g. after a db change).
        // Also make sure that labels and schema links are consistent w/latest db & trackDb.
        var dataSources = mutState.getIn(['hgi_querySpec', 'dataSources']) || Immutable.List();
        var addDsInfo = mutState.get('addDsInfo');
        if (dataSources && addDsInfo) {
            var newDataSources = dataSources.filter(function(ds) {
                var trackPath = ds.get('trackPath');
                return trackPath && this.isInGroupedTrackDb(mutState, trackPath);
            }, this
                                                    ).map(function(ds) {
                var trackPath = ds.get('trackPath');
                ds = ds.set('label', this.labelFromTrackPath(mutState, trackPath));
                ds = ds.set('schemaUrl', this.schemaUrlFromTrackPath(mutState, trackPath));
                return ds;
            }, this);
            if (! newDataSources.equals(dataSources)) {
                mutState.setIn(['hgi_querySpec', 'dataSources'], newDataSources);
            }
            this.disableDataSourcesInAddDsMenus(mutState);
            this.updateAddDsDisable(mutState);
        }
    },

    changeRange: function(mutState, uiPath, newValue) {
        // User changed 'region to annotate' select; if they changed it to 'define regions'
        // but have not yet defined any regions, open the UserRegions popup.
        var existingRegions = mutState.getIn(['regionSelect', 'userRegions', 'regions']);
        if (newValue === 'userRegions' && (!existingRegions || existingRegions === "")) {
            this.openUserRegions(mutState);
        } else {
            this.changeCartString(mutState, uiPath, newValue);
        }
    },

    clearUserRegions: function(mutState) {
        // Clear user region state and reset hgi_range.
        this.changeCartString(mutState, ['regionSelect', 'hgi_range'], 'position');
        this.clearUserRegionState(mutState);
    },

    uploadedUserRegions: function(mutState) {
        // Wait for results of parsing uploaded region file
        mutState.setIn(['regionSelect', 'loading'], true);
    },

    pastedUserRegions: function(mutState) {
        // Update hgi_range to select user regions
        mutState.setIn(['regionSelect', 'hgi_range'], 'userRegions');
    },

    cartSendQuerySpec: function(mutState) {
        // When some part of querySpec changes, update the cart variable.
        var state = mutState || this.state;
        var querySpec = state.get('hgi_querySpec').toJS();
        // Remove UI-only fields from dataSources:
        querySpec.dataSources = _.map(querySpec.dataSources, function(ds) {
            return _.omit(ds, ['schemaUrl', 'label']);
        });
        this.cartSet('hgi_querySpec', JSON.stringify(querySpec));
    },

    changeAddDsMenu: function(mutState, uiPath, newValue) {
        // User changed group, track, view, or subtrack in the 'Add Data Source' section.
        // Changing group or track (or view) has side effects on lower-level menus.
        var ix = uiPath.pop();
        var trackPath = this.getAddDsTrackPath(mutState);
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
        this.groupedTrackDbToMenus(mutState, trackPath, ix);
        // Store updated trackPath in state and cart
        trackPath = this.getAddDsTrackPath(mutState);
        mutState.set('hgi_addDsTrackPath', trackPath);
        this.cartSet('hgi_addDsTrackPath', JSON.stringify(trackPath.toJS()));
    },

    addDataSource: function(mutState) {
        // User clicked button to add a new data source (track/table); tell server and render,
        // unless they have reached the max number of allowed data sources, or are trying to
        // add a data source that has already been added.
        if (mutState.getIn(['hgi_querySpec', 'dataSources']).size >= this.maxDataSources) {
            alert("Sorry, you can choose a maximum of " + this.maxDataSources + " data sources.");
            return;
        }
        var addDsTrackPath = this.getAddDsTrackPath(mutState);
        var label = this.labelFromTrackPath(mutState, addDsTrackPath);
        var alreadyAdded = false;
        mutState.getIn(['hgi_querySpec', 'dataSources']).forEach(function (ds) {
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
        var schemaUrl = this.schemaUrlFromTrackPath(mutState, addDsTrackPath);
        var dataSource = Immutable.fromJS({ trackPath: addDsTrackPath,
                                            label: label,
                                            schemaUrl: schemaUrl});
        mutState.updateIn(['hgi_querySpec', 'dataSources'], function(list) {
            return list.push(dataSource);
        });
        this.disableDataSourcesInAddDsMenus(mutState);
        this.updateAddDsDisable(mutState);
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
        mutState.updateIn(['hgi_querySpec', 'dataSources'], function(dataSources) {
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
                mutState.updateIn(['hgi_querySpec', 'dataSources'], function(oldList) {
                    return oldList.remove(ix);
                });
                this.disableDataSourcesInAddDsMenus(mutState);
                this.updateAddDsDisable(mutState);
                this.cartSendQuerySpec(mutState);
            } else if (action && action === 'moreOptions') {
                var track = mutState.getIn(['hgi_querySpec', 'dataSources', ix, 'track']);
                alert('more options ' + track);
            } else {
                this.error('dataSourceClick: unrecognized path', path);
            }
        }
    },

    doChooseFields: function(mutState) {
        // User clicked the 'Choose fields' button -- get field info from server.
        var dataSources = mutState.getIn(['hgi_querySpec', 'dataSources']);
        var tables = dataSources.map(function(dataSource) {
            return dataSource.get('trackPath').last();
        });
        this.cartDo({ getFields: { tables: tables.join(',') } });
    },

    doFieldSelect: function(mutState, path, newValue) {
        // User clicked something on the field selection popup
        if (path[2] === 'remove') {
            mutState.set('tableFields', null);
        } else {
            var table = path[2], field = path[3];
            if (field) {
                // Update field's status in UI state and hgi_querySpec
                mutState.setIn(['tableFields', table, 'fields', field, 'checked'], newValue);
                mutState.setIn(['hgi_querySpec', 'outFileOptions', 'tableFields',
                                     table, field], newValue);
            } else {
                // 'Set all' or 'Clear all' according to newValue
                mutState.updateIn(['tableFields', table, 'fields'], function(fieldVals) {
                    return fieldVals.map(function(checkedAndDesc) {
                        return checkedAndDesc.set('checked', newValue);
                    });
                });
                // hgi_querySpec might not yet contain any explicit choices for table's fields,
                // so iterate over the complete set of fields in top-level tableFields:
                mutState.getIn(['tableFields', table, 'fields']
                               ).forEach(function(checkedAndDesc, field) {
                    mutState.setIn(['hgi_querySpec', 'outFileOptions', 'tableFields',
                                         table, field], checkedAndDesc.get('checked'));
                });
            }
            this.cartSendQuerySpec(mutState);
        }
    },

    doGetOutput: function(mutState) {
        // User clicked 'Get output' button; make a form and submit it.
        var querySpec = mutState.get('hgi_querySpec').toJS();
        // Remove UI-only fields from dataSources:
        querySpec.dataSources = _.map(querySpec.dataSources, function(ds) {
            return _.omit(ds, ['schemaUrl', 'label']);
        });
        var doFile = mutState.getIn(['hgi_querySpec', 'outFileOptions', 'doFile']);
        if (querySpec.dataSources.length < 1) {
            alert('Please add at least one data source.');
        } else {
            if (! doFile) {
                // Show loading image and message that the query might take a while.
                mutState.set('showLoadingImage', true);
            }
            querySpec = encodeURIComponent(JSON.stringify(querySpec));
            $('input[name="hgi_querySpec"]').val(querySpec);
            $('#queryForm')[0].submit();
        }
    },

    outFileOptionsClick: function(mutState, path, newValue) {
        // User clicked on something in OutFileOptions; path[1] is either an action or
        // the piece of state that needs to be updated on server
        if (path[1] === 'chooseFields') {
            this.doChooseFields(mutState);
        } else if (path[1] === 'fieldSelect') {
            this.doFieldSelect(mutState, path, newValue);
        } else if (path[1] === 'getOutput') {
            this.doGetOutput(mutState);
        } else {
            mutState.setIn(['hgi_querySpec'].concat(path), newValue);
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
        this.cartDo({ cgiVar: this.getChangeDbCgiVars(mutState),
                      get: { 'var': 'position,hgi_querySpec' },
                      getGeneSuggestTrack: {},
                      getUserRegions: {}
                      });
        this.cartDo({ cgiVar: this.getChangeDbCgiVars(mutState),
                      getGroupedTrackDb: { fields: this.tdbFields,
                                           excludeTypes: this.excludeTypes } });
    },

    initialize: function() {
        // Register handlers for cart variables that need special treatment:
        this.registerCartVarHandler(['hgi_querySpec', 'hgi_range', 'tableFields',
                                     'userRegionsUpdate'],
                                    this.handleCartVar);
        this.registerCartVarHandler('hgi_addDsTrackPath', this.handleJsonBlob);
        this.registerCartVarHandler('groupedTrackDb', this.handleGroupedTrackDb);
        this.registerCartValidateHandler(this.validateCart);
        // Register handlers for UI events:
        this.registerUiHandler(['regionSelect', 'hgi_range'], this.changeRange);
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
                          get: {'var': 'hgi_range,hgi_querySpec,hgi_addDsTrackPath'},
                          getUserRegions: {}
                        });
            // The groupedTrackDb structure is so large for hg19 and other well-annotated
            // genomes that the volume of compressed JSON takes a long time on the wire.
            this.cartDo({ getGroupedTrackDb: { fields: this.tdbFields,
                                               excludeTypes: this.excludeTypes }
                        });
        }
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
