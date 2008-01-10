class AddLoaderFieldsToProjectTypes < ActiveRecord::Migration
  def self.up
    add_column :project_types, :loader, :string
    add_column :project_types, :load_params, :string
    add_column :project_types, :load_time_out, :integer
  end

  def self.down
    remove_column :project_types, :loader, :string
    remove_column :project_types, :load_params, :string
    remove_column :project_types, :load_time_out, :integer
  end
end
