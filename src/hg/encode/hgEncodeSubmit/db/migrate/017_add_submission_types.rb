class AddSubmissionTypes < ActiveRecord::Migration
  def self.up
    create_table :submission_types do |t|
      t.column :name, :string
      t.column :short_label, :string
      t.column :long_label, :string
      t.column :validator, :string
      t.column :type_params, :string
      t.column :description, :string
      t.column :display_order, :integer
      t.column :time_out, :integer
    end
  end

  def self.down
    drop_table :submission_types
  end
end
