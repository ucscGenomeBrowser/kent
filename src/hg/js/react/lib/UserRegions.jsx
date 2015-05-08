/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate, Modal, TextArea */
var pt = React.PropTypes;

var UserRegions = React.createClass({
    // Let the user paste/upload BED or position-style regions.
    // Uses jquery.bifrost plugin for ajax uploading even in IE 8 & 9.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'cancel')            called when user clicks cancel
    // update(path + 'uploaded')          called when user clicks submit with upload file
    // update(path + 'pasted', newValue)  called when user clicks submit with text input

    propTypes: { // Optional
                 settings: pt.object   // expected: Immutable { enabled: bool, regions: string }
               },

    getInitialState: function() {
        // Keep regions as local state until user is done with this popup.
        // Keep track of whether user is sending file or pasted text.
        var settings = this.props.settings;
        var initialRegions = settings ? settings.get('regions') : "";
        return { regions: initialRegions,
                 sendingFile: false };
    },

    setRegions: function(newValue) {
        // Set local state's regions.
        this.setState({ regions: newValue });
    },

    getRegions: function() {
        // Get local state's regions.
        return this.state.regions;
    },

    setSendingFile: function(newValue) {
        // Update local state's notion of whether we're sending file or pasted data.
        this.setState({ sendingFile: newValue });
    },

    getSendingFile: function() {
        // Get local state's notion of whether we're sending file or pasted data.
        return this.state.sendingFile;
    },

    componentWillReceiveProps: function(nextProps) {
        // Update local state when a new value is handed down from above.
        // We can't do this during render because then the user's changes are ignored!
        var newRegions = nextProps.settings.get('regions');
        if (this.state.regions !== newRegions) {
            this.setState({regions: newRegions});
        }
    },

    onChangeFile: function(ev) {
        // User selected a file to upload; clear textarea regions because they will be ignored
        // if a file is uploaded.
        console.log("onChangeFile", ev.target.files);
        this.setRegions('');
        this.setSendingFile(true);
    },

    resetFileInput: function() {
        // Reset the file input to clear a previous selection.
        // This is very unReactlike, to just mutate a DOM element with JQuery, but it should
        // be OK here because React and the model don't manage the file input state because
        // in order to support IE8 and IE9 we're using jquery.bifrost to ajax the file.
        var $fileInput = $('input[type="file"]');
        $fileInput.wrap('<'+'form'+'>').closest('form').get(0).reset();
        $fileInput.unwrap();
    },

    updateTextArea: function(path, newValue) {
        // User entered some regions in the TextArea; reflect their changes in local state.
        // Also reset the file input because user can send one or the other, not both.
        console.log("updateTextArea", newValue);
        this.setRegions(newValue);
        this.setSendingFile(false);
        this.resetFileInput();
    },

    onClear: function() {
        // User clicked clear; clear the local state and file input but don't tell model yet.
        this.setRegions('');
        this.resetFileInput();
    },

    onCancel: function() {
        // User clicked cancel; tell the model to close this
        console.log("onCancel");
        this.props.update(this.props.path.concat('cancel'));
    },

    onSubmit: function() {
        // Now send the pasted or uploaded regions to the server, and tell model to close this.
        console.log("onSubmit");
        if (this.getSendingFile()) {
            this.props.update(this.props.path.concat('uploaded'));
        } else {
            this.props.update(this.props.path.concat('pasted'), this.getRegions());
        }
    },

    render: function() {
        if (this.props.settings && this.props.settings.get('enabled')) {
            return (
                <Modal title='Paste/Upload Regions'
                       path={this.props.path} update={this.props.update}>
                  <br />
                  Upload file: <input type='file' name='regionFile'
                                      onChange={this.onChangeFile} />
                  <br />
                  Or paste regions: <span style={{width: '10em'}} />
                  <br />
                  <TextArea value={this.getRegions()}
                            name='regionText'
                            rows={10} cols={70}
                            path={this.props.path} update={this.updateTextArea} />
                  <br />
                  <input type='button' value='submit' onClick={this.onSubmit}/>
                  <input type='button' value='clear' onClick={this.onClear} />
                  <input type='button' value='cancel' onClick={this.onCancel} />
                  <br />
                  <p>
                    Regions may be either uploaded from a local file or entered
                    in the "Paste regions" input.  Selecting a local file will
                    clear the "Paste regions" input, and entering regions in
                    the "Paste regions" input will clear the file selection.
                  </p>
                  <p>
                    Regions may be formatted as 3- or 4-field
                    <a className='reactLink' target="_blank"
                       href="http://genome.ucsc.edu/FAQ/FAQformat.html#format1">BED</a>
                    file format.
                    For example:
                  </p>
                  <pre>
#  comment lines can be included starting with the # symbol{'\n'}
chrX   151073054   151173000{'\n'}
chrX   151183000   151190000  optionalRegionName{'\n'}
chrX   151283000   151290000{'\n'}
chrX   151383000   151390000{'\n'}
                  </pre>
                  <p>
                    BED start coordinates are 0-based, i.e. the first base of a
                    chromosome is 0.
                    The fourth field, name, is optional and is used only in
                    the Table Browser's correlation function.
                  </p>
                  <p>
                    Regions may also be formatted as coordinate ranges:
                    <pre>
chrX:151,283,001-151,290,000  optionalRegionName
                    </pre>
                    These are 1-relative coordinates, i.e. the first base of a
                    chromosome is 1.
                  </p>
                </Modal>
            );
        } else {
            return null;
        }
    }

});

// Without this, jshint complains that UserRegions is not used.  Module system would help.
UserRegions = UserRegions;
