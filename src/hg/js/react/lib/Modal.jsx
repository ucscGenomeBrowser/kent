/** @jsx React.DOM */
var pt = React.PropTypes;

function getScrollHeight() {
    // From https://developer.mozilla.org/en-US/docs/Web/API/Window.scrollY
    // FIXME: This belongs in a regular .js lib file.
    var supportPageOffset = window.pageXOffset !== undefined;
    var isCSS1Compat = ((document.compatMode || "") === "CSS1Compat");
    return supportPageOffset ? window.pageYOffset : isCSS1Compat ? document.documentElement.scrollTop : document.body.scrollTop;
}

var Modal = React.createClass({
    // A big popup pane that appears on top of the page, for use in special modes (hence "modal").
    // Don't treat this as Immutable because it wraps around children that have their own props.

    mixins: [PathUpdate],
    // update(path + 'remove') called when user clicks X icon to hide this

    propTypes: { title: pt.renderable.isRequired,  // title string or React.DOM object
               },

    defaultPopupStyle: { position: 'absolute',
                     display: 'inline-block',
                     top: '75px',
                     left: '75px',
                     width: '80%',
                     height: '80%',
                     zIndex: '100',
                     backgroundColor: '#FFFEE8',
                     borderStyle: 'solid',
                     borderWidth: '2px',
                     borderColor: 'black',
                     padding: '10px',
                     overflow: 'auto'
                   },

    render: function() {
        var thisPopupStyle = this.defaultPopupStyle;
        thisPopupStyle.top = getScrollHeight() + 75; // px is default unit in React style
        var path = this.props.path || [];
        return (
            <div style={thisPopupStyle}>
              <div>
                <span style={{float: "left"}}>
                  {this.props.title}
                </span>
                <Icon type="remove" extraClass = "removeButton floatRight"
                      path={path.concat('remove')} update={this.props.update} />
                <div style={{clear: 'both'}} />
                </div>
                {this.props.children}
              </div>
        );
    }
});
