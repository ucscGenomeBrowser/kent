class CreateProjectStatusLog < ActiveRecord::Migration
  def self.up
    create_table :project_status_log do |t|
      t.column :project_id, :integer
      t.column :status, :string
      t.column :time, :timestamp
    end
  end

  def self.down
    drop_table :project_status_log
  end
end
