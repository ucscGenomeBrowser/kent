/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdateOptional */
var pt = React.PropTypes;

var Icon = React.createClass({
    // Use the FontAwesome css library to make a pretty icon.

    mixins: [PathUpdateOptional, ImmutableUpdate],
    // update(path) called when user clicks icon (if update is included in props)

    propTypes: { type: pt.oneOf(['plus', 'spinner',
                                 'times', 'remove', 'x', // synonyms for 'X' icon
                                 'arrows-v', 'upDown'    // synonyms for up&down arrows
                                ]).isRequired,           // --> what kind of icon

                 // Optional
                 className: pt.string,                  // extra class(es) to apply
               },

    onClick: function() {
        if (this.props.update) {
            this.props.update(this.props.path);
        }
    },

    render: function() {
        var faClass = 'fa ';
        switch (this.props.type) {
            case 'plus':
                faClass += 'fa-plus';
                break;
            case 'spinner':
                faClass += 'fa-spinner fa-spin';
                break;
            case 'times':
            case 'remove':
            case 'x':
                faClass += 'fa-times';
                break;
            case 'arrows-v':
            case 'upDown':
                faClass += 'fa-arrows-v';
                break;
            default:
                console.warn('Icon: Unrecognized type "' + this.props.type + '"');
        }
        faClass += ' iconButton';
        if (this.props.className) {
            faClass += ' ' + this.props.className;
        }
        return (
            <i className={faClass} onClick={this.onClick}/>
        );
    }
});

// Without this, jshint complains that Icon is not used.  Module system would help.
Icon = Icon;
