/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate */
var pt = React.PropTypes;

var CheckboxLabel = React.createClass({
    // Make a checkbox followed by a label.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path, checked) called when user clicks checkbox

    propTypes: { // Optional
                 checked: pt.bool,           // is checkbox checked?  (initially)
                 label: pt.string,           // text to the right of checkbox
                 className: pt.string,       // class(es) to pass to input
                 style: pt.object            // style to pass to input
               },

    getDefaultProps: function () {
        return { checked: false,
                 label: ''
               };
    },

    handleClick: function(e) {
        // Extract new value from ReactDOM event and update model.
        var newVal = e.target.checked;
        this.props.update(this.props.path, newVal);
    },
    
    render: function() {
        return (
          <div key={this.props.ref} 
               className={this.props.className} style={this.props.style} >
            <input type='checkbox' checked={this.props.checked}
                   onChange={this.handleClick} />
            {this.props.label}
          </div>
        );
      }
});

// Without this, jshint complains that CheckboxLabel is not used.  Module system would help.
CheckboxLabel = CheckboxLabel;
