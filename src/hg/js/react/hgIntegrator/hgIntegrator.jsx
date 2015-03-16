/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate, CheckboxLabel, CladeOrgDb, Icon, LabeledSelect */
/* global LoadingImage, Modal, PositionSearch, Section, SetClearButtons, Sortable, TextInput */

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
    // update(path + 'hgi_range') called when user changes genome/position select

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
        if (posInfo.get('hgi_range') !== 'genome') {
            positionInput = <PositionSearch positionInfo={posInfo}
                                            className='sectionItem'
                                            db={props.db}
                                            path={props.path} update={props.update}
                            />;
        }
        return (
            <div className='sectionRow'>
              <LabeledSelect label='region to annotate'
                             className='sectionItem'
                             selected={posInfo.get('hgi_range')} options={this.menuOptions}
                             update={props.update} path={props.path.concat('hgi_range')} />
              {positionInput}
            </div>
        );
    }
}); // RegionOrGenome

var LabeledSelectRow = React.createClass({
    // Build a row of LabeledSelect's from an Immutable.List of Immutable descriptor objects
    // like {label, valLabels, selected} (or null, in which case skip to the next descriptor).

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + ix) called when user changes the ixth menu

    propTypes: { descriptors: pt.object.isRequired, // Immutable.List[{ valLabels, selected }]
               },

    makeMenuFromDescriptor: function(descriptor, ix) {
        // Make a LabeledSelect using fields of descriptor
        if (! descriptor || descriptor.get('hide')) {
            return null;
        } else {
            var key = 'lsrMenu' + ix;
            return (
                <LabeledSelect label={descriptor.get('label')}
                               selected={descriptor.get('selected')}
                               options={descriptor.get('valLabels')}
                               key={key}
                               className='sectionItem'
                               update={this.props.update} path={this.props.path.concat(ix)} />
            );
        }
    },

    render: function() {
        var descriptors = this.props.descriptors;
        return (
          <div className='sectionRow sectionItem'>
            {descriptors.map(this.makeMenuFromDescriptor).toJS()}
          </div>
        );
    }
}); // LabeledSelectRow

function makeSchemaLink(schemaUrl) {
    // Return a React component link to hgTables' schema page.
    if (schemaUrl) {
        return <span className='smallText sectionItem'>
            <a href={schemaUrl} target="ucscSchema"
               title="Open table schema in new window">
              View table schema
            </a></span>;
    } else {
        return null;
    }
}

var AddDataSource = React.createClass({
    // A section-lite with group/track/table (or someday group/composite/view/etc) selects
    // and a button to add the selected track/table

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'addDataSource') called when user clicks Add button
    // update(path + 'addDsMenuSelect' + ix) called when user changes the ixth menu
    // update(path + 'trackHubs') called when user clicks track hubs button
    // update(path + 'customTracks') called when user clicks custom tracks button

    propTypes: { addDsInfo: pt.object.isRequired   // Immutable obj w/List of menu descriptors,
                                                   // hgTables schema URL, & disabled flag
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
        var addDsInfo = this.props.addDsInfo;
        if (! (addDsInfo && addDsInfo.size)) {
            // Still waiting for data from server
            return <Icon type='spinner' />;
        }

        var schemaLink = makeSchemaLink(addDsInfo.get('schemaUrl'));
        return (
            <div>
              <div className='bigBoldText sectionRow'>
                Add Data Source
              </div>
              <LabeledSelectRow descriptors={addDsInfo.get('menus')}
                                path={path.concat('addDsMenuSelect')}
                                update={this.props.update} />
              {schemaLink}
              <input type='button' value='Add'
                     disabled={addDsInfo.get('disabled')}
                     onClick={this.onAdd} />
              <br />
              <div className='sectionRow'>
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
            var title = <div className='bigBoldText sectionRow'>Choose fields</div>;
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
                 showLoadingImage: pt.bool,    // If true, show loading image
                 disableGetOutput: pt.bool,         // If true, disable Get output button
                 disableGetOutputMessage: pt.node   // If disableGetOutput, show this message
               },

    getDefaultProps: function() {
        return { showLoadingImage: false };
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
              <div className='sectionRow'>
                <CheckboxLabel checked={doFile} label='Send output to file'
                               className='sectionItem'
                               path={path.concat('doFile')} update={this.props.update} />
                <div style={{display: fileInputDisplay}}>
                  <span className='sectionItem'>name:</span>
                  <span className='sectionItem'>
                    <TextInput value={fileName}
                               path={path.concat('fileName')} update={this.props.update}
                               size={75} />
                  </span>
                  <CheckboxLabel checked={doGzip} label='Compress with gzip (.gz)'
                                 className='sectionItem'
                                 path={path.concat('doGzip')} update={this.props.update} />
                </div>
              </div>
              <div className='sectionRow'>
                <input type='button' value='Choose fields...' onClick={this.onChooseFields} />
              </div>
              <FieldSelect fieldInfo={this.props.fieldInfo}
                           update={this.props.update} path={path.concat('fieldSelect')} />
              <div className='sectionRow'>
                <br />
                <input type='button' value='Get output' onClick={this.onGetOutput} />
              </div>
              <LoadingImage loading={this.props.showLoadingImage} />
            </div>
        );
    }
}); // OutFileOptions

var QueryBuilder = React.createClass({
    // Interface for adding and configuring data sources and output options

    mixins: [PathUpdate, ImmutableUpdate],
    // update() calls: see OutFileOptions; also:
    // update(path + 'dataSources' + 'reorder'): user finished drag&drop of Sortable dataSource
    // update(path + 'dataSources' + i + 'remove'): remove the ith dataSource

    propTypes: { // Optional:
                 querySpec: pt.object,      // Data sources and output options
                 addDsInfo: pt.object,      // Options for adding a data source
                 tableFields: pt.object,    // If present, show 'Choose fields' modal
                 showLoadingImage: pt.bool  // If true, show loading image (for query execution)
               },

    renderDataSource: function(dataSource, i) {
        // Render a single dataSource ({ trackPath, label, schemaUrl }).
        var dsKey = 'ds' + i;
        var path = ['dataSources', i];
        var schemaLink = makeSchemaLink(dataSource.get('schemaUrl'));

        return (
            <div key={dsKey} className='dataSourceSubsection'>
                <div className='sortHandle'>
                  <span className='floatLeft'>
                    <Icon type='upDown' className='sectionItem'/>
                    <span className='bigBoldText sectionItem'>
                      {dataSource.get('label')}
                    </span>
                    <span className='sectionItem'>
                      {schemaLink}
                    </span>
                  </span>
                  <Icon type='x' className='floatRight'
                        update={this.props.update} path={path.concat('remove')} />
                  <div className='clear' />
                </div>
            </div>
        );
    },

    renderDataSources: function(dataSources) {
        // Wrap Sortable around rendered dataSources if we have enough data.
        if (dataSources && dataSources.size) {
            return (
                <Sortable sortableConfig={{ handle: '.sortHandle', axis: 'y' }}
                          path={['dataSources', 'reorder']} update={this.props.update}
                          className='subsectionBox' >
                  {dataSources.map(this.renderDataSource).toJS()}
                </Sortable>
            );
        } else {
            return (
                <div className='subsectionBox'>
                  <span className='disabledMessage'>please add at least one data source</span>
                </div>
            );
        }
    },

    render: function() {
        var addDsInfo = this.props.addDsInfo;
        var querySpec = this.props.querySpec;
        if (! (addDsInfo && querySpec)) {
            // Waiting for data from server
            return <Section title='Loading...' />;
        } else {
            var dataSources = querySpec.get('dataSources');
            var outputInfo = querySpec.get('outFileOptions') || Immutable.Map();
            var tableFields = this.props.tableFields;
            var disableGetOutput = (! (dataSources && dataSources.size));
            var disableGetOutputMessage =
              <span className='disabledMessage'>
                At least one data source must be added.
              </span>;
            return (
              <div>
                <Section title='Configure Data Sources'>
                  {this.renderDataSources(dataSources)}
                  <AddDataSource addDsInfo={addDsInfo}
                                 path={[]} update={this.props.update} />
                </Section>

                <Section title='Output Options'>
                  <OutFileOptions options={outputInfo}
                                  fieldInfo={tableFields}
                                  showLoadingImage={this.props.showLoadingImage}
                                  disableGetOutput={disableGetOutput}
                                  disableGetOutputMessage={disableGetOutputMessage}
                                  path={['outFileOptions']} update={this.props.update}
                                  />
                </Section>
              </div>
            );
        }
    }
}); // QueryBuilder

var DbPosAndQueryBuilder = React.createClass({
    // Container for selecting a species, configuring position/genome, and building a query.

    mixins: [PathUpdate, ImmutableUpdate],
    // update() calls: see CladeOrgDb, RegionOrGenome and QueryBuilder

    propTypes: { // Optional:
                 cladeOrgDbInfo: pt.object, // See CladeOrgDb
                 positionInfo: pt.object,   // See RegionOrGenome
                 querySpec: pt.object,      // Data sources and output options
                 addDsInfo: pt.object,      // Options for adding a data source
                 tableFields: pt.object,    // If present, show 'Choose fields' modal
                 showLoadingImage: pt.bool  // If true, show loading image (for query execution)
    },

    render: function() {
        var path = this.props.path;
        var cladeOrgDbInfo = this.props.cladeOrgDbInfo;
        if (! cladeOrgDbInfo) {
            // Waiting for initial data from server
            return <Section title='Loading...' />;
        } else {
            return (
              <div>
                <Section title='Select Genome Assembly and Region'>
                  <CladeOrgDb menuData={cladeOrgDbInfo}
                              path={path.concat('cladeOrgDb')} update={this.props.update}/>
                  <RegionOrGenome positionInfo={this.props.positionInfo}
                                  db={cladeOrgDbInfo.get('db')}
                                  path={path.concat('positionInfo')} update={this.props.update}/>
                </Section>

                <QueryBuilder addDsInfo={this.props.addDsInfo}
                              querySpec={this.props.querySpec}
                              tableFields={this.props.tableFields}
                              showLoadingImage={this.props.showLoadingImage}
                              path={path} update={this.props.update}
                              />
              </div>
            );
        }
    }
}); // dbPosAndQueryBuilder


var AppComponent = React.createClass({
    // AnnoGrator interface

    mixins: [ImmutableUpdate],

    getDefaultProps: function() {
        return { path: [] };
    },

    render: function() {
        var appState = this.props.appState;
        var appStateJS = appState.toJS();
        console.log('top-level render:', appStateJS);
        var path = this.props.path;
        var helpText = appState.get('helpText') || '';
        return (
            <div className='sectionContents'>
              <span className='bigBoldText sectionRow sectionItem'>Annotation Integrator</span>
              <input type='button' value='Undo'
                     onClick={this.props.undo} disabled={!appState.get('canUndo')} />
              <input type='button' value='Redo'
                     onClick={this.props.redo} disabled={!appState.get('canRedo')} />

              <DbPosAndQueryBuilder cladeOrgDbInfo={appState.get('cladeOrgDb')}
                                    positionInfo={appState.get('positionInfo')}
                                    addDsInfo={appState.get('addDsInfo')}
                                    querySpec={appState.get('hgi_querySpec')}
                                    tableFields={appState.get('tableFields')}
                                    showLoadingImage={appState.get('showLoadingImage')}
                                    path={path} update={this.props.update}
                                    />

              <Section title='Using the Annotation Integrator'>
                <div dangerouslySetInnerHTML={{__html: helpText}} />
              </Section>
            </div>
        );
    }

});

// Without this, jshint complains that AppComponent is not used.  Module system would help.
AppComponent = AppComponent;
