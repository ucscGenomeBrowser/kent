/* global ImmutableUpdate, PathUpdate */

var SetClearButtons = React.createClass({
    // Return a div with buttons labeled "Set all" and "Clear all"; when clicked,
    // update model with path and boolean value to which all appropriate values should be set

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path, newValue):   called when user clicks 'Set all' (newValue=true) or 'Clear all'

    onSetAll: function() {
        this.props.update(this.props.path, true);
    },

    onClearAll: function() {
        this.props.update(this.props.path, false);
    },

    render: function() {
        return (
            <div>
              <input type='button' value='Set all' onClick={this.onSetAll} />
              <input type='button' value='Clear all' onClick={this.onClearAll} />
            </div>
        );
    }

});

// Without this, jshint complains that SetClearButtons is not used.  Module system would help.
SetClearButtons = SetClearButtons;
