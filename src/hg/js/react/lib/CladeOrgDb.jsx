/** @jsx React.DOM */
var pt = React.PropTypes;

var CladeOrgDb = React.createClass({
    // LabeledSelect's for clade, org and db.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + clade|org|db, newValue) called when user changes clade|org|db

    propTypes: { menuData: pt.object.isRequired,  // Immutable.Map { 
                                                  //   clade: x, cladeOptions: (LabeledSelect opts)
                                                  //   org: x, orgOptions: ..., db...}
               },

    render: function() {
        var menuData = this.props.menuData;
        var path = this.props.path || [];
        return (
            <div>
              <LabeledSelect label='group' selected={menuData.get('clade')}
                             options={menuData.get('cladeOptions')}
                             update={this.props.update} path={path.concat('clade')} />
              <LabeledSelect label='genome' selected={menuData.get('org')}
                             options={menuData.get('orgOptions')}
                             update={this.props.update} path={path.concat('org')} />
              <LabeledSelect label='assembly' selected={menuData.get('db')}
                             options={menuData.get('dbOptions')}
                             update={this.props.update} path={path.concat('db')} />
            </div>
        );
    }
});
