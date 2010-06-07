class DropSTypeSubmission < ActiveRecord::Migration
  def self.up
    remove_column :submissions, :s_type
  end

  def self.down
    add_column :submissions, :s_type, :string
  end
end
