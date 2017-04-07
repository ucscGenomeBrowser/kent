/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate, CladeOrgDb, Icon, LabeledSelect */
///* global LoadingImage, Modal, PositionSearch, Section, SetClearButtons, Sortable, TextInput */
/* global  Section, Sortable */
///* global UserRegions */

var pt = React.PropTypes;

// AnnoGrator interface.

/*
var RegionSelect = React.createClass({
    // Let the user choose between position/search term, whole genome, or user-defined regions.
    // Modules PositionSearch and UserRegions handle the details.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'position', newValue) called when user changes position
    // update(path + 'hidePosPopup') called when user clicks to hide popup
    // update(path + 'positionMatch', matches): user clicks position link in popup
    //                                          (matches obj is from hgFind)
    // update(path + 'hgi_range') called when user changes genome/position select
    // update(path + 'changeRegions') called when user clicks to change pasted/uploaded regions
    // update(path + 'clearRegions') called when user clicks to reset pasted/uploaded regions

    propTypes: { regionSelect: pt.object.isRequired,  // expected to be Immutable {
                 //   disableGenome (bool): disable genome-wide query option
                 //   hgi_range: position | genome | userRegions
                 //   loading (bool): display spinner (e.g. while uploading file)
                 //   positionInfo: PositionSearch's expected Immutable state
                 //   userRegions: UserRegions' expected Immutable state
                 // }
                 db: pt.string // must be given if positionInfo includes geneSuggestTrack
               },

    menuOptions: Immutable.fromJS([ { label: 'position or search term', value: 'position' },
                                    { label: 'genome', value: 'genome'},
                                    { label: 'defined regions', value: 'userRegions'}
                                    ]),

    menuOptionsNoGenome: Immutable.fromJS([ { label: 'position or search term', value: 'position' },
                                            { label: 'genome', value: 'genome', disabled: true },
                                            { label: 'defined regions', value: 'userRegions' }
                                    ]),

    changeRegions: function() {
        // user clicked to edit pasted/uploaded regions
        this.props.update(this.props.path.concat('changeRegions'));
    },

    clearRegions: function() {
        // user clicked to reset pasted/uploaded regions
        this.props.update(this.props.path.concat('clearRegions'));
    },

    render: function() {
        var props = this.props;
        var regionSelect = props.regionSelect;
        var userRegions = regionSelect.get('userRegions');
        var selected = regionSelect.get('hgi_range');
        var disableGenome = regionSelect.get('disableGenome');
        var menuOptions = disableGenome ? this.menuOptionsNoGenome : this.menuOptions;
        var modeControls = null;
        if (selected === 'userRegions') {
            modeControls = [
                <span className='smallText sectionItem'>{userRegions.get('summary')}</span>,
                <input type='button' value='change regions' onClick={this.changeRegions} />,
                <input type='button' value='clear' onClick={this.clearRegions} />
                ];
        } else if (selected !== 'genome') {
            modeControls = <PositionSearch positionInfo={regionSelect.get('positionInfo')}
                                           className='sectionItem'
                                           db={props.db}
                                           path={props.path.concat('positionInfo')}
                                           update={props.update}
                                           />;
        }
        var spinner = null;
        if (regionSelect.get('loading')) {
            spinner = <Icon type="spinner" className="floatRight" />;
        }
        return (
            <div className='sectionRow'>
              <LabeledSelect label='region to annotate'
                             className='sectionItem'
                             selected={selected} options={menuOptions}
                             update={props.update} path={props.path.concat('hgi_range')} />
              {spinner}
              {modeControls}
              <UserRegions settings={userRegions}
                           update={props.update} path={props.path.concat('userRegions')} />
            </div>
        );
    }
}); // RegionSelect
*/

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

    onGetOutput: function() {
        // user click the "Get output" button
        var path = this.props.path || [];
        this.props.update(path.concat('getOutput'));
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
              <div className='boldText sectionRow'>
                Add Data Source
              <div className='sectionRow'>
                <input type='button' value='Add all visibile wiggles' 
                onClick={this.onGetOutput} />
              </div>
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
              <div className='sectionRow'>
                <input type='button' value='Get output' onClick={this.onGetOutput} />
              </div>
            </div>
        );
    }
}); // AddDataSource


/*
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
                 options: pt.object,        // should be Immutable.Map {doFile, fileName, doGzip}
                 fieldSelect: pt.object,    // table/field info from server following click on
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
              <FieldSelect fieldSelect={this.props.fieldSelect}
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
*/

var QueryBuilder = React.createClass({
    // Interface for adding and configuring data sources and output options

    mixins: [PathUpdate, ImmutableUpdate],
    // update() calls: see OutFileOptions; also:
    // update(path + 'dataSources' + 'reorder'): user finished drag&drop of Sortable dataSource
    // update(path + 'dataSources' + i + 'remove'): remove the ith dataSource

    propTypes: { // Optional:
                 dataSources: pt.object,    // Data sources (tracks)
                 outFileOptions: pt.object, // Output options
                 addDsInfo: pt.object,      // Options for adding a data source
                 fieldSelect: pt.object,    // If present, show 'Choose fields' modal
                 showLoadingImage: pt.bool  // If true, show loading image (for query execution)
               },

    renderDataSource: function(dataSource, i) {
        // Render a single dataSource ({ trackPath, label, schemaUrl }).
        var dsKey = 'ds' + i;
        var path = this.props.path.concat('dataSources', i);
        var schemaLink = makeSchemaLink(dataSource.get('schemaUrl'));

        return (
            <div key={dsKey} className='dataSourceSubsection'>
                <div className='sortHandle'>
                  <span className='floatLeft'>
                    <Icon type='upDown' className='sectionItem'/>
                    <span className='boldText sectionItem'>
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
        var reorderPath = this.props.path.concat('dataSources', 'reorder');
        if (dataSources && dataSources.size) {
            return (
                <Sortable sortableConfig={{ handle: '.sortHandle', axis: 'y' }}
                          path={reorderPath} update={this.props.update}
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
        var dataSources = this.props.dataSources;
        //var outputInfo = this.props.outFileOptions || Immutable.Map();
        if (! (addDsInfo && dataSources)) {
            // Waiting for data from server
            return <Section title='Loading...' />;
        } else {
         //   var fieldSelect = this.props.fieldSelect;
          //  var disableGetOutput = (! (dataSources && dataSources.size));
           // var disableGetOutputMessage =
            //  <span className='disabledMessage'>
             //   At least one data source must be added.
              //</span>;
            return (
              <div>
                <Section title='Configure Data Sources'>
                  {this.renderDataSources(dataSources)}
                  <AddDataSource addDsInfo={addDsInfo}
                                 path={[]} update={this.props.update} />
                </Section>

              </div>
            );
        }
    }
}); // QueryBuilder

var DbPosAndQueryBuilder = React.createClass({
    // Container for selecting a species, configuring position/genome, and building a query.

    mixins: [PathUpdate, ImmutableUpdate],
    // update() calls: see CladeOrgDb, RegionSelect and QueryBuilder

    propTypes: { // Optional:
                 cladeOrgDbInfo: pt.object, // See CladeOrgDb
                 regionSelect: pt.object,   // See RegionSelect
                 dataSources: pt.object,    // Data sources (tracks)
                 outFileOptions: pt.object, // Output options
                 addDsInfo: pt.object,      // Options for adding a data source
                 fieldSelect: pt.object,    // If present, show 'Choose fields' modal
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
                <Section title='Select Genome Assembly'>
                  <CladeOrgDb menuData={cladeOrgDbInfo}
                              path={path.concat('cladeOrgDb')} update={this.props.update}/>
                </Section>

                <QueryBuilder addDsInfo={this.props.addDsInfo}
                              dataSources={this.props.dataSources}
                              outFileOptions={this.props.outFileOptions}
                              fieldSelect={this.props.fieldSelect}
                              showLoadingImage={this.props.showLoadingImage}
                              path={path} update={this.props.update}
                              />
              </div>
            );
        }
    }
}); // dbPosAndQueryBuilder


/*
function helpSection() {
    return (
<Section title='Using the Data Integrator'>
<p>
The Data Integrator finds items in different tracks that overlap by position,
and unlike the Table Browser's intersection function, the Data
Integrator can output all fields from all selected tracks.  Up to 5
different tracks may be queried at a time.
</p>
<p>
This section contains a brief overview of Data Integrator controls.
For more information on using the tools,
see the <a href="../goldenPath/help/hgIntegratorHelp.html">Data
Integrator User's Guide</a>.
</p>

<p><b>Select Genome Assembly and Region</b>
<br />
The controls in this section are for selecting a genome assembly and region to search.
<ul>
  <li><b>group</b>:
      A species group: Mammal, Vertebrate, Insect etc.</li>
  <li><b>genome</b>:
      A single species such as Human or Mouse
      (not available for certain tracks with restrictions on data sharing).</li>
  <li><b>assembly</b>:
      A version of the reference genome assembly such as GRCh37/hg19.</li>
</ul>
</p>

<p><b>Configure Data Sources</b>
<br />
Currently selected data sources (tracks, custom tracks, hub tracks etc) are listed
with <Icon type="upDown" /> icons
for reordering the data sources.
The first data source is special in that data from the remaining data sources appear
only when they overlap with the first data source.
Under &quot;<b>Add Data Source</b>&quot;, several menus display available data sources:
<ul>
  <li><b>track group</b>:
      A category of data track, for example &quot;Genes and Gene Prediction&quot;
      or &quot;Regulation&quot;.</li>
  <li><b>track</b>:
      One or more data tables containing results of an experiment
      or a group of closely related experiments.
      Some tracks are not available when the region is set
      to <b>genome</b> due to the data provider's restrictions on sharing.</li>
  <li><b>table</b>:
      This appears only when the selected track has more than one data table.</li>
</ul>
These sections can be reordered by dragging on the section title or arrow icon on the left.
To remove a section, click
the <Icon type="x" /> icon
to the right of the title.
Click on the Add button to add a new data source.
</p>

<p><b>Output Options</b>
<br />
<ul>
  <li><b>Send output to file</b>:
      check this box to have output sent to a local file
      instead of to the web browser window.
      <br />
      When this is checked, additional options appear:
      <ul>
        <li><b>name</b>:
            the file name to which output will be saved</li>
        <li><b>compress with gzip</b>:
            check this box to have the output file compressed by gzip (.gz).
            This saves disk space and may reduce network transfer time.</li>
    </ul>
  </li>
  <li><b>Choose fields</b>:
      click this button to pop up a dialog box with a checkbox
      for each field of each data source.  If a checkbox is checked, that field will
      appear in the output.
      Some tracks also have related mysql tables that can be selected and added to output.
      However, some of those tables may be unavailable when region is set
      to <b>genome</b> due to the data provider's restrictions on sharing.</li>
</ul>
</p>
</Section>
);
}
*/

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
        return (
            <div className='cgiContents'>
              <div className='cgiTitleBox'>
                <span className='cgiTitle'>Composite Builder</span>
                <input type='button' value='Undo'
                       onClick={this.props.undo} disabled={!appState.get('canUndo')} />
                <input type='button' value='Redo'
                       onClick={this.props.redo} disabled={!appState.get('canRedo')} />
              </div>

              <DbPosAndQueryBuilder cladeOrgDbInfo={appState.get('cladeOrgDb')}
                                    regionSelect={appState.get('regionSelect')}
                                    addDsInfo={appState.get('addDsInfo')}
                                    dataSources={appState.get('dataSources')}
                                    outFileOptions={appState.get('outFileOptions')}
                                    fieldSelect={appState.get('fieldSelect')}
                                    showLoadingImage={appState.get('showLoadingImage')}
                                    path={path} update={this.props.update}
                                    />

            </div>
        );
    }

});

// Without this, jshint complains that AppComponent is not used.  Module system would help.
AppComponent = AppComponent;
