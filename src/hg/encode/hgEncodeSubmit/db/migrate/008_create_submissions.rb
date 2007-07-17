class CreateSubmissions < ActiveRecord::Migration
  def self.up
    create_table :submissions do |t|
      t.column :name, :string
      t.column :user_id, :integer
      t.column :type, :string
      t.column :status, :string
      t.column :created_at, :timestamp
      t.column :updated_at, :timestamp
    end
  end

  def self.down
    drop_table :submissions
  end
end
