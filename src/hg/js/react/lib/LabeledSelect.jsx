/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate */
var pt = React.PropTypes;

var LabeledSelect = React.createClass({
    // A select input with a label that appears above it.  Reports change w/path to app model.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path, newValue) called when user changes selection

    propTypes: { label: pt.string.isRequired,  // label that appears above select input
                 // Optional:
                 selected: pt.string,          // initial selected value
                 options: pt.object,           // Immutable.List [ .Map{value: x, label: y,
                                               //                       disabled: false}, ... ]
                 className: pt.string          // class(es) to pass to wrapper div
               },

    optionFromValueLabel: function(item) {
        var value = item.get('value'), label = item.get('label'), disabled = item.get('disabled');
        return <option value={value} key={value} disabled={disabled}>{label}</option>;
    },

    onChange: function(e) {
        var newValue = e.target.value;
        this.props.update(this.props.path, newValue);
    },

    render: function() {
        var opts = null;
        if (this.props.options) {
            opts = this.props.options.map(this.optionFromValueLabel).toJS();
        }
        return (
            <div className={this.props.className}>
              <div>
                {this.props.label}
              </div>
              <select className='groupMenu' value={this.props.selected}
                      onChange={this.onChange}>
	        {opts}
              </select>
            </div>
        );
    }
});

// Without this, jshint complains that LabeledSelect is not used.  Module system would help.
LabeledSelect = LabeledSelect;
