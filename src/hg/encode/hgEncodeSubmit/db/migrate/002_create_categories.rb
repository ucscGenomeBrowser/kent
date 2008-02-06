class CreateCategories < ActiveRecord::Migration
 
  def self.up
    
    create_table :categories do |t|
      t.column :name, :string
    end
   
    # Because this table gets deleted later,
    # and the corresponding .rb file in app/models/ gets removed,
    # this causes the migration to fail when re-run on an
    # empty or sufficiently out-of-date db.
    # Instead of adding these back, it's probably
    # easier to just comment-out the problem statements: 
    #Category.create :name => "Electronics"
    #Category.create :name => "Real Estate"
    #Category.create :name => "Furniture"
    #Category.create :name => "Miscellaneous"
    
    add_column :classifieds, :category_id, :integer
    #Classified.find(:all).each do |c|
    #  c.update_attribute(:category_id, 4)
    #end
    
  end

  def self.down
    drop_table :categories
    remove_column :classifieds, :category_id
  end
  
end
