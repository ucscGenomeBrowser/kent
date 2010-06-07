class AddArchiveFileFields < ActiveRecord::Migration
  def self.up
    add_column :submissions, :file_name, :string
    add_column :submissions, :file_size, :string
    add_column :submissions, :file_date, :timestamp
  end

  def self.down
    remove_column :submissions, :file_name
    remove_column :submissions, :file_size
    remove_column :submissions, :file_date
  end
end
