/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate, Icon, Modal, TextInput */
var pt = React.PropTypes;

var SpeciesSearch = React.createClass({
    // Text input for position or search term, optionally with autocomplete if
    // props includes geneSuggestTrack, and a PositionPopup (defined below)
    // for multiple position/search matches.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'position', newValue) called when user changes position
    // update(path + 'hidePosPopup') called when user clicks to hide popup
    // update(path + 'positionMatch', matches): user clicks position link in popup
    //                                          (matches obj is from hgFind)

    propTypes: {   // Immutable.Map {
        //   position: initial value of position input
        //   loading (bool): display spinner next to position input
        //   positionMatches (Immutable.Vector of Maps): multiple search results for popup

        // Optional
        className: pt.string,   // class(es) to pass to wrapper div
        org: pt.string, // name of organism currently selected (ex. Human)
        db: pt.string, // name of database currently selected (ex. hg38)
    },

    // No-op update function for TextInput - we don't want blur to trigger model updates,
    // only the autocomplete selection should update the model via onSpeciesSelect
    noOpUpdate: function() {},

    onSpeciesSelect: function(selectEle, item) {
        if (item.disabled || !item.genome) return;
        selectEle.innerHTML = item.label;
        this.props.update(this.props.path, item.genome);
    },

    onSearchError: function(jqXHR, textStatus, errorThrown, term) {
        return [{label: 'No genomes found', value: '', genome: '', disabled: true}];
    },

    componentDidMount: function() {
        // If we have a geneSuggest track, set up autocomplete.
        var inputNode, $input;
        inputNode = this.refs.input.getDOMNode();
        var sel = document.getElementById("genomeLabel");
        var boundSel = this.onSpeciesSelect.bind(null, sel);
        initSpeciesAutoCompleteDropdown(inputNode.id, boundSel, null, null, null, this.onSearchError);
    },

    render: function() {
        return (
            <div className={this.props.className}>
              <label className="genomeSearchLabelDefault" for="genomeSearch">
                Change selected genome:
              </label>
              <div className="searchBarAndButton">
                  <TextInput id='speciesSearchInput' placeholder="Search for any species, genome or assembly name"
                             size={45} ref='input' update={this.noOpUpdate} />
                  <div className="searchCell">
                    Current Genome:
                    <span id="genomeLabel">{this.props.org + " (" + this.props.db + ")"}</span>
                  </div>
              </div>
            </div>
        );
    }

}); // SpeciesSearch

// Without this, jshint complains that SpeciesSearch is not used.  Module system would help.
SpeciesSearch = SpeciesSearch;
