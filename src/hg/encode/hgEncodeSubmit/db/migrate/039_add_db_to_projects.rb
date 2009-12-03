class AddDbToProjects < ActiveRecord::Migration
  def self.up
    add_column :projects, :db, :string
  end

  def self.down
    remove_column :projects, :db
  end
end
