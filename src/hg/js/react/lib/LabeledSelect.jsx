/** @jsx React.DOM */
var pt = React.PropTypes;

var LabeledSelect = React.createClass({
    // A select input with a label that appears above it.  Reports change w/path to app model.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path, newValue) called when user changes selection

    propTypes: { label: pt.string.isRequired,  // label that appears above select input
                 // Optional:
                 selected: pt.string,          // initial selected value
                 options: pt.object            // Immutable.Vector [ .Map{value: x, label: y}, ... ]
               },

    optionFromValueLabel: function(item) {
        var value = item.get('value'), label = item.get('label');
        return <option value={value} key={value}>{label}</option>;
    },

    onChange: function(e) {
        var newValue = e.target.value;
        this.props.update(this.props.path, newValue);
    },

    render: function() {
        var opts = null;
        if (this.props.options)
            opts = this.props.options.map(this.optionFromValueLabel).toJS();
        return (
            <div className='topLabelSelect'>
              <label style={{ display: 'block' }}>{this.props.label}</label>
              <select className='groupMenu' value={this.props.selected} onChange={this.onChange}>
	        {opts}
              </select>
            </div>
        );
    }
});
