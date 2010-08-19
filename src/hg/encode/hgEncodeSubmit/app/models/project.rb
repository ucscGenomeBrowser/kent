class Project < ActiveRecord::Base

  belongs_to :user
  belongs_to :project_type
  has_many :project_archives, :order => 'archive_no', :dependent => :destroy

  validates_presence_of :name
  validates_presence_of :project_type_id
  validates_presence_of :status
  validates_presence_of :user_id
  validates_uniqueness_of   :name
  #validates_presence_of :archives_active # caused problems for new project create

  # This works, but I don't even know if I need it.
  #  adding the increment in the main even is easy enough
  def getNextArchiveNo()
    self.archive_count += 1
    save
    #update_attributes(:archive_count => archive_count)
    return self.archive_count 
  end

end
