var HgAiModel = ImModel.extend({
    // Handle hgAi's UI state and events and interaction with the server/CGI/cart.

    mixins: [ CladeOrgDbMixin(['cladeOrgDb']),
              PositionSearchMixin(['positionInfo']) ],

    handleCartVar: function(cartVar, newValue) {
        // Some cart variables require special action (not simply being merged into top-level state)
        if (cartVar === 'hgai_querySpec') {
            if (newValue === '') {
                // No querySpec in cart yet -- make an empty one for rendering:
                this.mutState.set(cartVar,
                                  Immutable.fromJS({ dataSources: [], outFileOptions: {} }));
            } else {
                newValue = decodeURIComponent(newValue);
                newValue = JSON.parse(newValue);
                this.mutState.set(cartVar, Immutable.fromJS(newValue));
            }
        } else if (cartVar === 'hgai_range') {
            // Bundle this with positionInfo for fast immutable-prop checking
            this.mutState.setIn(['positionInfo', cartVar], newValue);
        } else if (cartVar === 'trackDbInfo') {
            var trackDbInfo = newValue;
            this.mutState.set(cartVar, Immutable.fromJS(trackDbInfo));
            // Update addDsTrackPath to something that we know for sure is in this db
            var firstGroup = trackDbInfo.groupOptions[0].value;
            var firstTrack = trackDbInfo.groupTracks[firstGroup][0].value;
            var firstTable = trackDbInfo.trackTables[firstTrack][0];
            this.mutState.set('addDsTrackPath',
                              Immutable.Map({ group: firstGroup, track: firstTrack,
                                              table: firstTable }));
        } else if (cartVar === 'tableFields') {
            // The server has sent an object mapping table names to lists of fields.
            // Construct an immutable map of table names to ordered maps of fields to
            // booleans for checkboxes; if hgai_querySpec contains saved settings,
            // use those; otherwise true unless field is bin.
            var tfDefaults = {};
            var current = this.mutState.getIn(['hgai_querySpec', 'outFileOptions', 'tableFields']);
            if (current) {
                current = current.toJS();
            } else {
                current = {};
            }
            Object.keys(newValue).forEach(function(table) {
                var info = newValue[table];
                var currentFieldSettings = current[table] || {};
                var newFieldSettings = Immutable.OrderedMap();
                info.fields.forEach(function(field) {
                    var checked = currentFieldSettings[field];
                    if (checked === undefined) {
                        checked = (field !== 'bin');
                    }
                    newFieldSettings = newFieldSettings.set(field, checked);
                }, this);
                tfDefaults[table] = Immutable.Map({ fields: newFieldSettings, label: info.label});
            }, this);
            this.mutState.set('tableFields', Immutable.OrderedMap(tfDefaults));
        } else {
            this.error('handleCartVar: unexpectedly given', cartVar, newValue);
        }
    },

    isInTrackDb: function(dataSource) {
        // Search trackDbInfo with dataSource's track/table; return true if found.
        var track = dataSource.get('track');
        var table = dataSource.get('table');
        var tableList = this.mutState.getIn(['trackDbInfo', 'trackTables', track]);
        return tableList && tableList.find(function(listedTable) {
            return listedTable === table;
        });
    },

    validateCart: function() {
        // We usually get trackDbInfo and hgai_query in the same server response.
        // When both have been incorporated into mutState, trim any data sources
        // from hgai_query that aren't found in trackDbInfo (e.g. after a db change).
        var dataSources = this.mutState.getIn(['hgai_querySpec', 'dataSources']) || [];
        var newDataSources = dataSources.filter(function(ds) {
            return this.isInTrackDb(ds);
        }, this);
        if (! newDataSources.equals(dataSources)) {
            this.mutState.setIn(['hgai_querySpec', 'dataSources'], newDataSources);
        }
    },

    cartSendQuerySpec: function() {
        // When some part of querySpec changes, update the cart variable.
        var state = this.mutState || this.state;
        this.cartSet('hgai_querySpec', JSON.stringify(state.get('hgai_querySpec').toJS()));
    },

    changeAddDsTrackPath: function(path, newValue) {
        // User changed group, track or table in the 'Add Data Source' section.
        // Changing group or track has side effects on lower-level menus.
        this.mutState.setIn(path, newValue);
        var which = path.pop();
        var newTrack, newTable;
        if (which === 'group') {
            newTrack = this.mutState.getIn(['trackDbInfo', 'groupTracks', newValue, 0, 'value']);
            newTable = this.mutState.getIn(['trackDbInfo', 'trackTables', newTrack, 0]);
            this.mutState.setIn(path.concat('track'), newTrack);
            this.mutState.setIn(path.concat('table'), newTable);
        } else if (which === 'track') {
            newTable = this.mutState.getIn(['trackDbInfo', 'trackTables', newValue, 0]);
            this.mutState.setIn(path.concat('table'), newTable);
        } else if (which !== 'table') {
            console.error('changeAddDsTrackPath: bad path "' + which + '"');
        }
    },

    addDataSource: function() {
        // User clicked button to add a new data source (track/table); tell server and render.
        var clonedTrackPath = Immutable.fromJS(this.mutState.get('addDsTrackPath').toJS());
        this.mutState.updateIn(['hgai_querySpec', 'dataSources'], function(list) {
            return list.push(clonedTrackPath);
        });
        this.cartSendQuerySpec();
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

    reorderDataSources: function(path, reordering) {
        // User dragged and dropped a Data Source section to a new position; update accordingly.
        // reordering is an array whose indices are new positions and values are old positions.
        this.mutState.updateIn(['hgai_querySpec', 'dataSources'], function(dataSources) {
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
        this.cartSendQuerySpec();
    },

    dataSourceClick: function(path, data) {
        // User clicked on something in a DataSource
        if (path[1] === 'reorder') {
            this.reorderDataSources(path, data);
        } else if (path.length < 2) {
            this.error('appModel.click: path has dataSources but not ix:', path);
        } else {
            var ix = path[1], action = path[2];
            if (action && action === 'remove') {
                this.mutState.updateIn(['hgai_querySpec', 'dataSources'], function(oldList) {
                    return oldList.remove(ix);
                });
                this.cartSendQuerySpec();
            } else if (action && action === 'moreOptions') {
                var track = this.mutState.getIn(['hgai_querySpec', 'dataSources', ix, 'track']);
                alert('more options ' + track);
            } else {
                this.error('dataSourceClick: unrecognized path', path);
            }
        }
    },

    doChooseFields: function() {
        // User clicked the 'Choose fields' button -- get field info from server.
        var dataSources = this.mutState.getIn(['hgai_querySpec', 'dataSources']);
        var tables = dataSources.map(function(trackPath) {
            return trackPath.get('table');
        });
        this.cartDo({ getFields: { tables: tables.join(',') } });
    },

    doFieldSelect: function(path, newValue) {
        // User clicked something on the field selection popup
        if (path[2] === 'remove') {
            this.mutState.set('tableFields', null);
        } else {
            var table = path[2], field = path[3];
            if (field) {
                // Update field's status in UI state and hgai_querySpec
                this.mutState.setIn(['tableFields', table, 'fields', field], newValue);
                this.mutState.setIn(['hgai_querySpec', 'outFileOptions', 'tableFields',
                                     table, field], newValue);
            } else {
                // 'Set all' or 'Clear all' according to newValue
                this.mutState.updateIn(['tableFields', table, 'fields'], function(fieldVals) {
                    return fieldVals.map(function() { return newValue; });
                });
                // hgai_querySpec might not yet contain any explicit choices for table's fields,
                // so iterate over the complete set of fields in top-level tableFields:
                this.mutState.getIn(['tableFields', table, 'fields']).forEach(function(val, field) {
                    this.mutState.setIn(['hgai_querySpec', 'outFileOptions', 'tableFields',
                                         table, field], val);
                }.bind(this));
            }
            this.cartSendQuerySpec();
        }
    },

    doGetOutput: function() {
        // User clicked 'Get output' button; make a form and submit it.
        var hgsid = window.hgsid || 0;
        var querySpec = this.mutState.get('hgai_querySpec').toJS();
        if (querySpec.dataSources.length < 1) {
            alert('Please add at least one data source.');
        } else {
            this.mutState.set('submitted', true);
            querySpec = encodeURIComponent(JSON.stringify(querySpec));
            $('input[name="hgai_querySpec"]').val(querySpec);
            $('#queryForm')[0].submit();
        }
    },

    outFileOptionsClick: function(path, newValue) {
        // User clicked on something in OutFileOptions; path[1] is either an action or
        // the piece of state that needs to be updated on server
        if (path[1] === 'chooseFields') {
            this.doChooseFields();
        } else if (path[1] === 'fieldSelect') {
            this.doFieldSelect(path, newValue);
        } else if (path[1] === 'getOutput') {
            this.doGetOutput();
        } else {
            this.mutState.setIn(['hgai_querySpec'].concat(path), newValue);
            this.cartSendQuerySpec();
        }
    },

    initialize: function() {
        // Register handlers for cart info and ui events:
        this.registerCartVarHandler(['hgai_querySpec', 'hgai_range', 'tableFields', 'trackDbInfo'],
                                    this.handleCartVar);
        this.registerCartValidateHandler(this.validateCart);
        this.registerUiHandler(['positionInfo', 'hgai_range'], this.changeCartVar);
        this.registerUiHandler(['addDsTrackPath'], this.changeAddDsTrackPath);
        this.registerUiHandler(['addDataSource'], this.addDataSource);
        this.registerUiHandler(['trackHubs'], this.goToTrackHubs);
        this.registerUiHandler(['customTracks'], this.goToCustomTracks);
        this.registerUiHandler(['dataSources'], this.dataSourceClick);
        this.registerUiHandler(['outFileOptions'], this.outFileOptionsClick);

        // Bootstrap initial state from page or by server request, then render:
        if (window.initialAppState) {
            this.state = Immutable.fromJS(window.initialAppState);
            this.render();
        } else {
            this.cartDo({ getCladeOrgDbPos: {},
                          getGroupsTracksTables: {},
                          getStaticHtml: {tag: 'helpText', file: 'goldenPath/help/hgAiHelp.html'},
                          get: {'var': 'hgai_range,hgai_querySpec'}
                        });
        }
    }


}); // HgAiModel

cart.setCgi('hgAi');

var container = document.getElementById('appContainer');

function render(state, update, undo, redo) {
    // ImModel calls this function after making changes to state.
    // Pass the immutable state object and callbacks to our top-level react component:
    React.render(React.createElement(AppComponent, {appState: state,
                                                    update: update, undo: undo, redo: redo }),
		 container);
}

var hgAiModel = new HgAiModel(render);
hgAiModel.initialize();
