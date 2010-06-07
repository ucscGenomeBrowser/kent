class NewEmailAddress < ActiveRecord::Migration
  def self.up
    add_column :users, :new_email, :string
    add_column :users, :email_activation_code, :string, :limit => 40
  end

  def self.down
    remove_column :users, :email_activation_code
    remove_column :users, :new_email
  end
end
