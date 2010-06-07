class AddUnloaderFieldsToProjectTypes < ActiveRecord::Migration
  def self.up
    add_column :project_types, :unloader, :string
    add_column :project_types, :unload_params, :string
    add_column :project_types, :unload_time_out, :integer
  end

  def self.down
    remove_column :project_types, :unloader, :string
    remove_column :project_types, :unload_params, :string
    remove_column :project_types, :unload_time_out, :integer
  end
end
