class AddStatusAgainToProjectArchives < ActiveRecord::Migration
  def self.up
    add_column :project_archives, :status, :string
  end

  def self.down
    remove_column :project_archives, :status
  end
end
