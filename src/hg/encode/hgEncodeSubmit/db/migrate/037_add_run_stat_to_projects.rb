class AddRunStatToProjects < ActiveRecord::Migration
  def self.up
    add_column :projects, :run_stat, :string
  end

  def self.down
    remove_column :projects, :run_stat
  end
end
