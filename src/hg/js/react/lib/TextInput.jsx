/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate */
var pt = React.PropTypes;

var ENTER_KEY = 13;

var TextInput = React.createClass({
    // Text input that keeps local state while user is typing.

    mixins: [PathUpdate, ImmutableUpdate],
        // update(path, value) called when user finishes changing value

    propTypes: { // Optional
                 value: pt.string,            // initial value of text input (before user changes)
                 ref: pt.string,              // React ref handle for parent to invoke this.refs[ref]
                 size: pt.number,             // size attribute to pass on to input element
                 className: pt.string,         // class(es) to pass to input
                 placeholder: pt.string       // placeholder value if any
               },

    getInitialState: function() {
        return {value: this.props.value};
    },

    onBlur: function(e) {
        // When user is done changing the input, notify model.
        var newValue = e.target.value.trim();
        newValue = newValue.replace(/\u2013|\u2014/g, '-');
        if (newValue !== this.props.value) {
            this.props.update(this.props.path, newValue);
        }
    },

    onKeyPress: function(e) {
        // Keep ordinary keypress updates local; notify parent only when user hits Enter.
        if (e.which === ENTER_KEY) {
            this.onBlur(e);
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
              <input id={this.props.id} size={this.props.size} value={this.state.value} ref={this.props.ref}
                     className={this.props.className}
                     placeholder={this.props.placeholder}
                     onKeyPress={this.onKeyPress}
                     onBlur={this.onBlur}
                     onChange={this.localOnChange} />
               );
    }
});

// Without this, jshint complains that TextInput is not used.  Module system would help.
TextInput = TextInput;
