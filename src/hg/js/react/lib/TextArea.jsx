/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate */
var pt = React.PropTypes;

var TextArea = React.createClass({
    // textarea that keeps local state while user is typing.

    mixins: [PathUpdate, ImmutableUpdate],
        // update(path, value) called when user finishes changing value

    propTypes: { // Optional
                 value: pt.string,        // initial value of text input (before user changes)
                 name: pt.string,         // name (for form to CGI)
                 rows: pt.number,         // rows attribute to pass on to textarea element
                 cols: pt.number,         // cols attribute to pass on to textarea element
                 ref: pt.string,          // React ref handle for parent to invoke this.refs[ref]
                 className: pt.string     // class(es) to pass to textarea element
               },

    getInitialState: function() {
        return {value: this.props.value};
    },

    onBlur: function(e) {
        // When user is done changing the input, notify model.
        var newValue = e.target.value.trim();
        if (newValue !== this.props.value) {
            this.props.update(this.props.path, newValue);
        }
    },

    localOnChange: function(e) {
        // Update to reflect the user's keypress.
        var newValue = e.target.value;
        this.setState({value: newValue});
    },

    componentWillReceiveProps: function(nextProps) {
        // Update local state when a new value is handed down from above.
        // We can't do this during render because then the user's changes are ignored!
        if (this.state.value !== nextProps.value) {
            this.setState({value: nextProps.value});
        }
    },

    render: function() {
        return (
              <textarea value={this.state.value}
                        name={this.props.name}
                        rows={this.props.rows}  cols={this.props.cols}
                        ref={this.props.ref}
                        className={this.props.className}
                        onBlur={this.onBlur}
                        onChange={this.localOnChange} />
               );
    }
});

// Without this, jshint complains that TextArea is not used.  Module system would help.
TextArea = TextArea;
