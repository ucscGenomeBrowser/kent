class AddWhodunnitToProjectStatusLogs < ActiveRecord::Migration
  def self.up
    add_column :project_status_logs, :who, :string
  end

  def self.down
    remove_column :project_status_logs, :who
  end
end
