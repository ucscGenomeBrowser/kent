class AddUserFieldsNamePiInst < ActiveRecord::Migration
  def self.up
    add_column :users, :name, :string
    add_column :users, :pi, :string
    add_column :users, :institution, :string
  end

  def self.down
    remove_column :users, :name
    remove_column :users, :pi
    remove_column :users, :institution
  end
end
