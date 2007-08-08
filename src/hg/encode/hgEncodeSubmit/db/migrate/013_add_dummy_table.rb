class AddDummyTable < ActiveRecord::Migration
  def self.up
    create_table :dummy do |t|
      t.column :user_id, :integer   # This is an id in the user table
      t.column :created_at, :timestamp 
      t.column :updated_at, :timestamp 
    end
  end

  def self.down
    drop_table dummy
  end
end
