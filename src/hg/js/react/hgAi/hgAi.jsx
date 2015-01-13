/** @jsx React.DOM */
var pt = React.PropTypes;

// AnnoGrator interface.

var RegionOrGenome = React.createClass({
    // Let the user choose between position/search term or whole genome.
    // Handle position input behavior: look up search terms on enter,

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'position', newValue) called when user changes position
    // update(path + 'hidePosPopup') called when user clicks to hide popup
    // update(path + 'positionMatch', matches): user clicks position link in popup
    //                                          (matches obj is from hgFind)
    // update(path + 'hgai_range') called when user changes genome/position select

    propTypes: { positionInfo: pt.object.isRequired,  // Immutable.Map {
                 //   position: initial value of position input
                 //   loading (bool): display spinner next to position input
                 //   positionMatches (Immutable.Vector of Maps): multiple search results for popup
                 //   geneSuggestTrack: optional track to use for autocomplete
                 // }
                 db: pt.string // must be given if positionInfo includes geneSuggestTrack
               },

    menuOptions: Immutable.fromJS([ { label: 'position or search term', value: 'position' },
                                    { label: 'genome', value: 'genome'}
                                    ]),

    render: function() {
        var props = this.props;
        var posInfo = props.positionInfo;
        var positionInput = null;
        if (posInfo.get('hgai_range') !== 'genome')
            positionInput = <PositionSearch positionInfo={posInfo}
                                            db={props.db}
                                            path={props.path} update={props.update}
                            />;
        return (
            <div style={{'marginTop': '5px'}}>
              <LabeledSelect label='region to annotate'
                             selected={posInfo.get('hgai_range')} options={this.menuOptions}
                             update={props.update} path={props.path.concat('hgai_range')} />
              {positionInput}
            </div>
        );
    }
}); // RegionOrGenome

var GroupTrackTable = React.createClass({
    // LabeledSelect's for group, track and table.
    //#*** This should be fancier than just g/t/t -- it should handle composites & views.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + group|track|table) called when user changes group|track|table

    propTypes: { trackPath: pt.object.isRequired,    // Immutable.Map { group, track, table }
                 trackDbInfo: pt.object.isRequired,  // Immutable.Map { groupOptions, groupTracks,
                                                     // trackTables }
    },

    render: function() {
        var props = this.props;
        var path = props.path || [];
        var group = props.trackPath.get('group');
        var track = props.trackPath.get('track');
        var table = props.trackPath.get('table');
        var groupOptions = props.trackDbInfo.get('groupOptions');
        var groupTracks = props.trackDbInfo.get('groupTracks');
        var trackTables = props.trackDbInfo.get('trackTables');
        var trackOptions = groupTracks.get(group);
        var tableNames = trackTables.get(track);
        if (! tableNames)
            tableNames = trackTables.get(trackOptions.getIn([0, 'value']));
        var tableOptions;
        if (tableNames) {
            tableOptions = tableNames.map(function(name) {
                return Immutable.Map({ label: name, value: name });
            });
        }
        var tableSelect;
        if (track !== table) {
            tableSelect =
              <LabeledSelect label='table' selected={table} options={tableOptions}
                             update={props.update} path={path.concat(['table'])} />;
        }
        return (
          <div style={{display: "inline-block"}}>
            <LabeledSelect label='track group' selected={group} options={groupOptions}
                           update={props.update} path={path.concat(['group'])} />
            <LabeledSelect label='track' selected={track} options={trackOptions}
                           update={props.update} path={path.concat(['track'])} />
            {tableSelect}
          </div>
        );
    }
}); // GroupTrackTable

var AddDataSource = React.createClass({
    // A section-lite with group/track/table (or someday group/composite/view/etc) selects
    // and a button to add the selected track/table

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'addDataSource') called when user clicks Add button
    // update(path + 'addDsTrackPath' + group|track|table) called when user changes group|track|table
    // update(path + 'trackHubs') called when user clicks track hubs button
    // update(path + 'customTracks') called when user clicks custom tracks button

    propTypes: { trackPath: pt.object.isRequired,    // currently selected group, track, table
                 trackDbInfo: pt.object.isRequired,  // Immutable.Map {
                                      //   groupOptions: pt.array, group menu options
                                      //   groupTracks: pt.object, maps groups to track menu options
                                      //   trackTables: pt.object  maps tracks to table lists
                 // Optional
                 schemaLink: pt.node                 // React component: link to hgTables schema
               },

    onAdd: function() {
        // Send path + 'addDataSource' to app model
        this.props.update(this.props.path.concat('addDataSource'));
    },

    onTrackHubs: function() {
        // Send path + 'trackHubs' to app model
        this.props.update(this.props.path.concat('trackHubs'));
    },

    onCustomTracks: function() {
        // Send path + 'customTracks' to app model
        this.props.update(this.props.path.concat('customTracks'));
    },

    render: function() {
        var path = this.props.path || [];
        var trackDbInfo = this.props.trackDbInfo;
        var groupTracks = trackDbInfo && trackDbInfo.get('groupTracks');
        if (! groupTracks) {
            // Still waiting for data from server
            return <Icon type='spinner' />;
        }

        return (
            <div>
              <div className='bigBoldText'>
                Add Data Source
              </div>
              <GroupTrackTable trackPath={this.props.trackPath}
                               trackDbInfo={trackDbInfo}
                               path={path.concat('addDsTrackPath')} update={this.props.update}
                               />
              {this.props.schemaLink}
              <input type='button' value='Add' onClick={this.onAdd} />
              <br />
              <div style={{marginTop: 5}}>
                get more data:<br />
                <input type='button' value='track hubs' onClick={this.onTrackHubs} />
                <input type='button' value='custom tracks' onClick={this.onCustomTracks} />
              </div>
            </div>
        );
    }
}); // AddDataSource

var FieldSelect = React.createClass({
    // Popup with checkboxes for selecting fields of some tables.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + table + field + 'checked', newValue):
    //                                   called when user clicks a field's checkbox
    // update(path + table, newValue):   called when user clicks 'Set all' or 'Clear all'
    // update(path + 'remove') called when user clicks X icon to hide this popup

    propTypes: { // Optional:
                 fieldInfo: pt.object,  // renders popup if truthy.
                                        // maps table to list of fields + checked state.
               },

    makeCheckboxGrid: function(table, fields) {
        // Make a checkbox for each field, labeled by field name.
        return _.map(fields, function(checked, field) {
            var path = this.props.path || [];
            path = path.concat(table, field, 'checked');
            return <CheckboxLabel key={table+'.'+field} checked={checked} label={field}
                                  path={path} update={this.props.update} />;
        }, this);
    },

    makeTableSections: function() {
        // For each table, make a section with the table's name followed by field checkboxes.
        var fieldInfo = this.props.fieldInfo.toJS();
        return _.map(fieldInfo, function(info, table) {
            return (
                <div key={table}>
                  <h3>{info.label}</h3>
                  <SetClearButtons path={this.props.path.concat(table)}
                                   update={this.props.update} />
                  {this.makeCheckboxGrid(table, info.fields)}
                </div>
            );
        }, this);
    },

    onDone: function() {
        // Close the Modal when user clicks Done button.
        this.props.update(this.props.path.concat('remove'));
    },

    render: function() {
        if (this.props.fieldInfo) {
            var title = <div className='bigBoldText'>Choose fields</div>;
            return (
                <Modal title={title} path={this.props.path} update={this.props.update}>
                  {this.makeTableSections()}
                  <br />
                  <input type='button' value='Done' onClick={this.onDone} />
                </Modal>
            );
        } else {
            return null;
        }
    }
}); // FieldSelect

var OutFileOptions = React.createClass({
    // Show output file options, with button to choose fields.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'doFile') called when user clicks file checkbox
    // update(path + 'fileName') called when user changes file name
    // update(path + 'doGzip') called when user clicks gzip checkbox
    // update(path + 'chooseFields') called when user clicks the select fields button
    // update(path + 'fieldSelect' + table + field + 'checked', newValue):
    //                                  called when user clicks a field's checkbox
    // update(path + 'fieldSelect' + 'remove')
    //                                  called when user clicks X icon to hide the fieldSelect popup
    // update(path + 'getOutput') called when user clicks the get output button

    propTypes: { // Optional:
                 options: pt.object,    // Immutable.Map {doFile, fileName, doGzip}
                 fieldInfo: pt.object,  // table/field info from server following click on
                                        // 'Choose fields' button
                 submitted: pt.bool,    // If true, show loading image
                 disableGetOutput: pt.bool,         // If true, disable Get output button
                 disableGetOutputMessage: pt.node   // If disableGetOutput, show this message
               },

    getDefaultProps: function() {
        return { submitted: false };
    },

    onChooseFields: function() {
        // user click the "Choose fields..." button
        var path = this.props.path || [];
        this.props.update(path.concat('chooseFields'));
    },

    onGetOutput: function() {
        // user click the "Get output" button
        var path = this.props.path || [];
        this.props.update(path.concat('getOutput'));
    },

    render: function() {
        var doFile = this.props.options.get('doFile');
        var fileName = this.props.options.get('fileName');
        var doGzip = this.props.options.get('doGzip');
        var path = this.props.path || [];
        var fileInputDisplay = doFile ? 'inline-block' : 'none';
        if (this.props.disableGetOutput) {
            return this.props.disableGetOutputMessage;
        }
        return (
            <div>
              <CheckboxLabel checked={doFile} label='Send output to file'
                             style={{display: 'inline-block'}}
                             path={path.concat('doFile')} update={this.props.update} />
              <div style={{display: fileInputDisplay, 'paddingLeft': '5px' }}>
                name:
                <TextInput value={fileName}
                           path={path.concat('fileName')} update={this.props.update}
                           size={75} />
                <CheckboxLabel checked={doGzip} label='Compress with gzip (.gz)'
                               style={{display: 'inline-block'}}
                               path={path.concat('doGzip')} update={this.props.update} />
              </div>
              <br />
              <input type='button' value='Choose fields...' onClick={this.onChooseFields} />
              <FieldSelect fieldInfo={this.props.fieldInfo}
                           update={this.props.update} path={path.concat('fieldSelect')} />
              <br />
              <br />
              <input type='button' value='Get output' onClick={this.onGetOutput} />
              <LoadingImage loading={this.props.submitted} />
            </div>
        );
    }
}); // OutFileOptions


function getLabelForValue(valLabelVec, val) {
    // Given an Immutable.Vector of Immutable.Map tuples of value and label, look for the
    // tuple with value equal to val and return the label from that tuple.
    // If no vector, or val is not found, return val as its own label.
    //#*** Libify?
    if (valLabelVec) {
        var ix;
        for (ix = 0;  ix < valLabelVec.size;  ix++) {
            if (valLabelVec.getIn([ix, 'value']) === val) {
                return valLabelVec.getIn([ix, 'label']);
            }
        }
    }
    return val;
}

function makeSchemaLink(db, group, track, table) {
    // Return a React component link to hgTables' schema page.
    var schemaUrl = 'hgTables?db=' + db + '&hgta_group=' + group + '&hgta_track=' + track +
                    '&hgta_table=' + table + '&hgta_doSchema=1';
    return <span style={{fontSize: 'small', paddingLeft: '10px', paddingRight: '10px'}}>
              <a href={schemaUrl} target="ucscSchema"
                 title="Open table schema in new window">
                View table schema
              </a></span>;
}

var AppComponent = React.createClass({
    // AnnoGrator interface

    mixins: [ImmutableUpdate],

    getDefaultProps: function() {
        return { path: [] };
    },

    renderDataSource: function(trackPath, i) {
        // Render a single dataSource ({group, track, table}).
        var trackPathKey = 'ds' + i;
        var path = ['dataSources', i];
        var group = trackPath.get('group');
        var track = trackPath.get('track');
        var table = trackPath.get('table');
        var groupTracks = this.props.appState.getIn(['trackDbInfo', 'groupTracks', group]);
        var trackLabel = getLabelForValue(groupTracks, track);
        if (track !== table) {
            trackLabel += ' (' + table + ')';
        }
        var db = this.props.appState.getIn(['cladeOrgDb', 'db']);
        var schemaLink = makeSchemaLink(db, group, track, table);

        var onMoreOptions = function (ev) {
            this.props.update(this.props.path.concat(['dataSources', i, 'moreOptions']));
        }.bind(this);

        return (
            <div key={trackPathKey} style={{'paddingTop': '5px', 'paddingBottom': '5px'}}>
                <div className='bigBoldText sortHandle'>
                  <span className='floatLeft'>
                    <Icon type='upDown' />
                    <span style={{'marginLeft': '3px'}}>
                      {trackLabel}
                      {schemaLink}
                    </span>
                  </span>
                  <Icon type='x' extraClass='floatRight'
                        update={this.props.update} path={path.concat('remove')} />
                  <div className='clear' />
                </div>
            </div>
        );
        //                <input type='button' value='More options...' onClick={onMoreOptions} />
    },

    renderDataSources: function(dataSources) {
        // Wrap Sortable around rendered dataSources if we have enough data.
        var boxStyle = {padding: 5, border: 'black solid 1px' };
        var emptyStyle = { padding: 5, color: 'gray', fontStyle: 'italic' };
        if (this.props.appState.getIn(['trackDbInfo', 'groupTracks']) && dataSources &&
            dataSources.size) {
            return (
                <Sortable sortableConfig={{ handle: '.sortHandle', axis: 'y' }}
                          path={['dataSources', 'reorder']} update={this.props.update}
                          style={boxStyle} >
                  {dataSources.map(this.renderDataSource).toJS()}
                </Sortable>
            );
        } else {
            return (
                <div style={boxStyle}>
                  <span style={emptyStyle}>please add at least one data source</span>
                </div>
            );
        }
    },

    render: function() {
        var appState = this.props.appState;
        var appStateJS = appState.toJS();
        console.log('top-level render:', appStateJS);
        var cladeOrgDbInfo = appState.get('cladeOrgDb'); // {clade|org|db}{,Options}
        var positionInfo = appState.get('positionInfo'); // selected=hgai_range, pos{,Matches,Loading, geneSuggestTrack
        var trackDbInfo = appState.get('trackDbInfo'); // groupOptions, groupTracks, trackTables
        var trackPath = appState.get('addDsTrackPath');
        var db = cladeOrgDbInfo.get('db');
        var schemaLink = makeSchemaLink(db, trackPath.get('group'), trackPath.get('track'),
                                        trackPath.get('table'));
        if (! (cladeOrgDbInfo && trackDbInfo && trackPath)) {
            // Waiting for initial data from server
            return <span>Loading...</span>;
        }
        var querySpec = appState.get('hgai_querySpec');
        var dataSources = querySpec.get('dataSources');
        var outputInfo = querySpec.get('outFileOptions') || Immutable.Map();
        var tableFields = appState.get('tableFields');
        var submitted = appState.get('submitted');
        var disableGetOutput = (! (dataSources && dataSources.size));
        var disableGetOutputMessage =
        <span style={{fontStyle: 'italic', color: 'gray', paddingLeft: 5}}>
          At least one data source must be added.
        </span>;
        var helpText = appState.get('helpText') || '';
        return (
            <div style={{margin: '5px'}}>
              <span className='bigBoldText'
                    style={{'paddingRight': '5px'}}>Annotation Integrator</span>
              <input type='button' value='Undo'
                     onClick={this.props.undo} disabled={!appState.get('canUndo')} />
              <input type='button' value='Redo'
                     onClick={this.props.redo} disabled={!appState.get('canRedo')} />
              <Section title='Select Genome Assembly and Region'>
                <CladeOrgDb menuData={cladeOrgDbInfo}
                            path={['cladeOrgDb']} update={this.props.update}/>
                <RegionOrGenome positionInfo={positionInfo} db={db}
                                path={['positionInfo']} update={this.props.update}/>
              </Section>

              <Section title='Configure Data Sources'>
                {this.renderDataSources(dataSources)}
                <AddDataSource trackPath={trackPath}
                               trackDbInfo={trackDbInfo}
                               schemaLink={schemaLink}
                               path={[]} update={this.props.update} />
              </Section>

              <Section title='Output Options'>
                <OutFileOptions options={outputInfo}
                                fieldInfo={tableFields}
                                submitted={submitted}
                                disableGetOutput={disableGetOutput}
                                disableGetOutputMessage={disableGetOutputMessage}
                                path={['outFileOptions']} update={this.props.update}
                                />
              </Section>

              <Section title='Using the Annotation Integrator'>
                <div dangerouslySetInnerHTML={{__html: helpText}} />
              </Section>

              { /* <pre>this.props.appState: {JSON.stringify(appStateJS, null, 2)}</pre> */ }
            </div>
        );
    }

});
