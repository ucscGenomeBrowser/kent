class AddSubTypesToSubmission < ActiveRecord::Migration
  def self.up
    add_column :submissions, :sub_type, :integer
  end

  def self.down
    remove_column :submissions, :sub_type
  end
end
