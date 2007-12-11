class PipelineController < ApplicationController

  require 'open-uri'

  #Process.wait
  #Process.waitpid2(0, Process::WNOHANG | Process::WUNTRACED)

  before_filter :login_required
  before_filter :check_user_is_owner, :except => [:new, :create, :list, :show_user, :show, :delete_archive ]
  
  layout 'standard'
  
  def list
    @projects = Project.find(:all)
    #@projectTypes = getProjectTypes
    @title = "These are the projects in our system"
  end
  
  def show_user

    #debug add a delay to simulate busyness.  is the db locked?
    #exitCode = system("sleep 20")

    @user = User.find(current_user.id)
    @projects = @user.projects
    #@projectTypes = getProjectTypes
    @title = "These are your projects"
    render :action => 'list'
    # not now using show_user.rhtml
  end
  
  def show
    @project = Project.find(params[:id])
    #@projectTypes = getProjectTypes
    if @project.status == "invalid"
      @errText = getErrText
    else
      if @project.status == "load failed"
        @errText = getLoadErrText
      else
	if @project.status == "upload failed"
	  @errText = getUploadErrText
	else
	  @errText = ""
    	end
      end
    end
    if @project.status.starts_with?("schedule expanding ")
      if process_uploaded_archive
        # ok
      end
    end
  end

  def valid_status
    @project = Project.find(params[:id])
    @errText = getErrText
  end

  def load_status
    @project = Project.find(params[:id])
    @errText = getLoadErrText
  end

  def upload_status
    @project = Project.find(params[:id])
    @errText = getUploadErrText
  end

  def begin_loading
    @project = Project.find(params[:id])
    if @project.status == "validated"
      @project.status = "schedule loading"
      @project.save
    end
    redirect_to :action => 'list'
  end

  def begin_validating
    @project = Project.find(params[:id])
    if @project.status == "uploaded"
      @project.status = "schedule validating"
      @project.save
    end
    redirect_to :action => 'list'
  end

  def new
    @project = Project.new
    @projectTypes = getProjectTypes
    #@projectTypesArray = @projectTypes.to_a.sort_by { |x| x[1]["displayOrder"] }
  end
  
  def create
    @project = Project.new(params[:project])
    @project.user_id = @current_user.id 
    @project.status = 'new'
    if @project.save
      redirect_to :action => 'list'
    else
      render :action => 'new'
    end
  end
  
  def edit
    @project = Project.find(params[:id])
    @projectTypes = getProjectTypes
    #@projectTypesArray = @projectTypes.to_a.sort_by { |x| x[1]["displayOrder"] }
  end
  
  def update
    @project = Project.find(params[:id])
    old_project_type_id = @project.project_type_id
    if @project.update_attributes(params[:project])
      if old_project_type_id != @project.project_type_id
        if @project.project_archives.length == 0
	  @project.status = "new"
	else
	  @project.status = "uploaded"
	end
	unless @project.save
	  flash[:warning] = "project record save failed"
	  return false
	end
      end
      redirect_to :action => 'show', :id => @project
    else
      render :action => 'edit'
    end
  end
  
  def delete
    projectDir=File.expand_path("#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}/#{@current_user.id}/#{@project.id}/")
    if File.exists?(projectDir)
      Dir.entries(projectDir).each { 
        |f| 
        fullName = File.join(projectDir,f)
        cmd = "rm -fr #{fullName}"
        exitCode = system(cmd)
        unless exitCode 
          flash[:warning] = "error cleaning up temporary upload subdirectory: <br>command=[#{cmd}], exitCode = #{exitCode}<br>"  
          return false
        end
      }
      Dir.delete(projectDir)
    end
    Project.find(params[:id]).destroy
    redirect_to :action => 'list'
  end
 
  def upload
    @project = Project.find(params[:id])
    return unless request.post?
    @upurl = params[:upload_url]
    @upload = params[:upload_file]
    if @upurl == "http://"
      @upurl = ""
    end
    return if @upload.blank? && @upurl.blank?
    #raise @upload.inspect
    #flash[:notice] = "#{@upload.inspect}"
    
    unless @upurl.blank?
      @filename = sanitize_filename(@upurl)
      @upload = open(@upurl)
    else
      @filename = sanitize_filename(@upload.original_filename)
      extensionsByMIME = {
        "application/zip" => ["zip", "ZIP"],
        "application/x-tar" => ["tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2"]
      }
      extensions = extensionsByMIME[@upload.content_type.chomp]
      unless extensions
        flash[:warning] = "invalid content_type=#{@upload.content_type.chomp}"
        return
      end
    end

    extensions = ["zip", "ZIP", "tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2"]
    unless extensions.any? {|ext| @filename.ends_with?("." + ext) }
      flash[:warning] = "File name <strong>#{@filename}</strong> is invalid. " +
        "Only a compressed archive file (zip,bz2,gz) is allowed"
      return
    end

    msg = ""

    # make sure parent paths exist
    projectDir = File.dirname(path_to_file)
    userDir = File.dirname(projectDir)
    Dir.mkdir(userDir) unless File.exists?(userDir)
    Dir.mkdir(projectDir) unless File.exists?(projectDir)

    nextArchiveNo = @project.archive_count+1

    plainName = @filename
    @filename = "#{"%03d" % nextArchiveNo}_#{@filename}"

    #debugging
    msg += "sanitized filename=#{@filename}<br>"
    msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    msg += "upload path=#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}<br>"
    msg += "path_to_file=#{path_to_file}<br>"
    msg += "nextArchiveNo=#{nextArchiveNo}<br>"

    # just in case, remove it it already exists (shouldn't happen)
    File.delete(path_to_file) if File.exists?(path_to_file)

    @project.status = "schedule uploading #{@upurl}"

    unless @upload.blank?
      if @upload.instance_of?(Tempfile)
        FileUtils.copy(@upload.local_path, path_to_file)
      else
        File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
      end
      @project.status = "schedule expanding #{plainName}"
    end

    unless @project.save
      flash[:warning] = "project record save failed"
      return
    end

    flash[:notice] = msg
    redirect_to :action => 'show', :id => @project

  end

  def uploadOld
    @project = Project.find(params[:id])
    return unless request.post?
    @upload = params[:upload_file]
    @upurl = params[:upload_url]
    if @upurl == "http://"
      @upurl = ""
    end

    return if @upload.blank? && @upurl.blank?
    #raise @upload.inspect
    #flash[:notice] = "#{@upload.inspect}"

    unless @upurl.blank?
      @filename = sanitize_filename(@upurl)
      @upload = open(@upurl)
    else
      @filename = sanitize_filename(@upload.original_filename)
    end

    extensionsByMIME = {
      "application/zip" => ["zip", "ZIP"],
      "application/x-tar" => ["tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2"]
    }
    extensions = extensionsByMIME[@upload.content_type.chomp]
    unless extensions
      flash[:warning] = "invalid content_type=#{@upload.content_type.chomp}"
      return
    end
    unless extensions && extensions.any? {|ext| @filename.ends_with?("." + ext) }
      flash[:warning] = "File name <strong>#{@filename}</strong> is invalid. " +
        "Only a compressed archive file (zip,bz2,gz) is allowed"
      # debug only next line:
      flash[:notice] = "content_type=#{@upload.content_type}" unless @upload.blank?
      return
    end


    msg = ""

    # make sure parent paths exist
    projectDir = File.dirname(path_to_file)
    userDir = File.dirname(projectDir)
    Dir.mkdir(userDir) unless File.exists?(userDir)
    Dir.mkdir(projectDir) unless File.exists?(projectDir)

    nextArchiveNo = @project.archive_count+1

    @filename = "#{"%03d" % nextArchiveNo}_#{@filename}"

    #debugging
    msg += "content_type=#{@upload.content_type}<br>"
    msg += "original_filename=#{@upload.original_filename}<br>" if @upurl.blank?
    msg += "sanitized filename=#{@filename}<br>"
    msg += "size=#{@upload.size}<br>"
    msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    msg += "upload path=#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}<br>"
    msg += "path_to_file=#{path_to_file}<br>"
    msg += "nextArchiveNo=#{nextArchiveNo}<br>"

    # just in case, remove it it already exists (shouldn't happen)
    File.delete(path_to_file) if File.exists?(path_to_file)

    @project.status = "uploading #{@filename}"

    unless @project.save
      flash[:warning] = "project record save failed"
      return
    end


    # fork here
    pid = Process.fork
    # if parent

    if pid
      msg += "child pid=#{pid}<br>"

      flash[:notice] = msg
      redirect_to :action => 'show', :id => @project
      return

    else

      # if child

      unless @upurl.blank?
        File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
      else
        if @upload.instance_of?(Tempfile)
          FileUtils.copy(@upload.local_path, path_to_file)
        else
          File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
        end
      end

      exit!(0)	
       # exit ? or return?

    end
   

  rescue OpenURI::HTTPError => err
    flash[:warning] = "HTTP Error: " + err.message
  end




  # This is the older method, see begin_validating above
  #  (this will be phased out and removed probably)
  def validate
    @project = Project.find(params[:id])
    @projectTypes = getProjectTypes
    msg = ""
    # make sure status is uploaded
    if @project.status == "new"
      msg += "Nothing uploaded yet"
      flash[:notice] = msg
      redirect_to :action => 'show', :id => @project
      return
    end

    # make sure parent paths exist
    projectDir = path_to_file
    userDir = File.dirname(projectDir)
    unless File.exists?(userDir) and File.exists?(projectDir)
      msg += "Unexpected error, userDir and projectDir do not exist!"
      flash[:warning] = msg
      redirect_to :action => 'show', :id => @project
      return
    end

    # make up an error output file
    @filename = "validate_error"
    errFile = path_to_file

    # run the validator
    cmd = ""
    cmd += @project.project_type.validator
    cmd += " "
    cmd += @project.project_type.type_params
    cmd += " "
    cmd += projectDir
    cmd += " 2>"
    cmd += errFile

    msg += cmd

    exitCode = run_with_timeout(cmd, 30)
    if exitCode >= 0
      msg += "<br>validate exitCode = #{exitCode}<br>" 
      if exitCode == 0
        if @project.status != "validated"
          @project.status = "validated"
          unless @project.save
            flash[:warning] = "project record save failed"
          end
        end
      else

        errText = File.open(errFile, "rb") { |f| f.read }
	flash[:warning] = "error:<br>" + errText
        
        if @project.status != "invalid"
          @project.status = "invalid"
          unless @project.save
            flash[:warning] = "project record save failed"
          end
        end
      end
    else
      flash[:warning] = "validate timeout exceeded<br>"  
      redirect_to :action => 'show', :id => @project
      return
    end 

    flash[:notice] = msg
    redirect_to :action => 'show', :id => @project

  end

  def delete_archive
    archive = ProjectArchive.find(params[:id])
    params[:id] = archive.project_id
    unless check_user_is_owner
      return
    end
    msg = ""
    archive.destroy
    msg += "Archive deleted"
    flash[:notice] = msg
    reexpand_all 
  end

  def reexpand_all
    @project = Project.find(params[:id])
    # make a hash of things to keep
    keepers = {}
    @project.project_archives.each do |a|
      keepers[a.file_name] = "keep"
    end
    # keep other special files
    keepers["validate_error"] = "keep"
    keepers["load_error"] = "keep"

    msg = ""
    # make sure parent paths exist
    projectDir = path_to_file

    # clean out directory (this will be handled more delicately later for re-uploading a project)
    Dir.entries(projectDir).each do 
      |f| 
      fullName = File.join(projectDir,f)
      unless keepers[f] or (f == ".") or (f == "..")
        
        # debug
        #msg += "projectDir: #{f} #{File.ftype(fullName)}<br>\n" 
        cmd = "rm -fr #{fullName}"
        exitCode = system(cmd)
        unless exitCode 
          flash[:warning] = "error cleaning up temporary upload subdirectory: <br>command=[#{cmd}], exitCode = #{exitCode}<br>"  
	  redirect_to :action => 'show', :id => @project
          return false
        end
      end
    end

    @project.project_archives.each do |a|
      a.project_files.each do |f|
        f.destroy
      end
    end
 
    @project.project_archives.each do |a|
      unless expand_archive(a)
        return
      end
    end

    if @project.project_archives.length == 0
      @project.status = "new"
    else
      @project.status = "uploaded"
    end
    unless @project.save
      flash[:warning] = "project record save failed"
      redirect_to :action => 'show', :id => @project
      return false
    end

    msg = "Cleaned out upload dir and re-expanded all archives"
    if flash[:notice]
      flash[:notice] += "<br>"+msg
    else
      flash[:notice] = msg
    end 
    redirect_to :action => 'show', :id => @project

  end

  # --------- PRIVATE ---------
private
  
  def check_user_is_owner
    @project = Project.find(params[:id])
    unless @project.user_id == @current_user.id
      flash[:warning] = "That project does not belong to you."
      redirect_to :action => 'list'
      return false
    end
    return true
  end

  # --- file upload routines ---

  def sanitize_filename(file_name)
    # get only the filename, not the whole path (from IE)
    just_filename = File.basename(file_name) 
    # replace all non-alphanumeric, underscore or periods with underscore
    just_filename.gsub(/[^\w\.\_]/,'_') 
  end

  def path_to_file
    # the expand_path method resolves this relative path to full absolute path
    File.expand_path("#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}/#{@current_user.id}/#{@project.id}/#{@filename}")
  end

  # --- read project types from config file into hash -------
  def getProjectTypes
    #open("#{RAILS_ROOT}/config/projectTypes.yml") { |f| YAML.load(f.read) }
    ProjectType.find(:all, :conditions => ['display_order != 0'], :order => "display_order")
  end

  # --- run process with timeout ---- (probably should move this to an application helper location)
  def run_with_timeout(cmd, myTimeout)
    # run process, kill it if exceeds specified timeout in seconds
    sleepInterval = 0.5  #seconds
    if ( (cpid = fork) == nil)
      exec(cmd)
    else
      before = Time.now
      while (true)
	pid, status = Process.wait2(cpid,Process::WNOHANG)
        if pid == cpid
          return status.exitstatus
        end
        if ( (Time.now - before) > myTimeout)
          Process.kill("ABRT",cpid)
	  pid, status = Process.wait2(cpid) # clean up zombies
          return -1
        end
        sleep(sleepInterval)
      end
    end
  end

  def expand_archive(archive)
    # expand archive belonging to archive record

    # make sure parent paths exist
    @filename = ""
    projectDir = path_to_file

    # make a temporary upload directory to unpack and merge from
    uploadDir = projectDir+"/upload"
    if File.exists?(uploadDir)
      cmd = "rm -fr #{uploadDir}"
      exitCode = system(cmd)
      unless exitCode 
        flash[:warning] = "command=[#{cmd}], exitCode = #{exitCode}<br>"  
        return false
      end
    end
    Dir.mkdir(uploadDir)

    @filename = archive.file_name
    # handle unzipping the archive
    if ["zip", "ZIP"].any? {|ext| @filename.ends_with?("." + ext) }
      cmd = "unzip -o  #{path_to_file} -d #{uploadDir}"     # .zip 
    else
      if ["gz", "GZ"].any? {|ext| @filename.ends_with?("." + ext) }
        cmd = "tar -xzf #{path_to_file} -C #{uploadDir}"  # .gz  gzip
      else  
        cmd = "tar -xjf #{path_to_file} -C #{uploadDir}"  # .bz2 bzip2
      end
    end

    exitCode = run_with_timeout(cmd, 8)
    unless exitCode >= 0
      flash[:warning] = "unzip timeout exceeded<br>"  
      return false
    end
 
    process_archive(archive.id, projectDir, uploadDir, "")

    # cleanup: delete temporary upload subdirectory
    if File.exists?(uploadDir)
      cmd = "rm -fr #{uploadDir}"
      exitCode = system(cmd)
      unless exitCode 
        flash[:warning] = "error cleaning up temporary upload subdirectory: <br>command=[#{cmd}], exitCode = #{exitCode}<br>"  
        return false
      end
    end

    if @project.project_archives.length == 0
      @project.status = "new"
    else
      @project.status = "uploaded"
    end

    unless @project.save
      flash[:warning] = "project record save failed"
      return false
    end

    return true

  end

  def my_join(path,name)
    if (path == "") 
      return name
    end
    if (name == "") 
      return path
    end
    return File.join(path,name)
  end

  def process_archive(archive_id, projectDir, uploadDir, relativePath)
    # process the archive files
    #msg += "uploadDir:<br>\n" 
    fullPath = my_join(uploadDir,relativePath)
    Dir.entries(fullPath).each do
      |f| 
      fullName = my_join(fullPath,f)
      if (File.ftype(fullName) == "directory")
        if (f != ".") and (f != "..")
          newRelativePath = my_join(relativePath,f)
          newDir = my_join(projectDir, newRelativePath)
	  unless File.exists?(newDir)
	    Dir.mkdir(newDir)
	  end
          process_archive(archive_id, projectDir, uploadDir, newRelativePath)
        end
      else 
        if File.ftype(fullName) == "file"
          #msg += "&nbsp;#{f}<br>\n"
          #unless ["bed", "idf", "adf", "sdrf" ].any? {|ext| f.downcase.ends_with?("." + ext) }
          #  flash[:warning] = "unknown file type: #{f}"
          #  return false
          #end 
   
	  relName = my_join(relativePath,f)
 
          # delete any equivalent projectFile records
  	  @project.project_archives.each do |c|
            old = ProjectFile.find(:first, :conditions => ['project_archive_id = ? and file_name = ?', c.id, relName])
            old.destroy if old
          end

          project_file = ProjectFile.new
          project_file.file_name = relName
          project_file.file_size = File.size(fullName)
          project_file.file_date = File.ctime(fullName)
          project_file.project_archive_id = archive_id 
          unless project_file.save
            flash[:warning] = "error saving project_file record for: #{f}"
            return false
          end
    
          parentDir = my_join(projectDir, relativePath)
          toName = my_join(parentDir, f)    
          # move file from temporary upload dir into parent dir
          File.rename(fullName,toName);

        end
      end
    end
  end

 def process_uploaded_archive

    @filename = @project.status.gsub(/^schedule expanding /,'')

    # make sure parent paths exist
    projectDir = File.dirname(path_to_file)
    userDir = File.dirname(projectDir)
    Dir.mkdir(userDir) unless File.exists?(userDir)
    Dir.mkdir(projectDir) unless File.exists?(projectDir)

    nextArchiveNo = @project.archive_count+1

    @filename = "#{"%03d" % nextArchiveNo}_#{@filename}"


    # dead code, just an example of using write_attribute:
    #write_attribute("file", path_to_file)

    # need to test for and delete any with same archive_no (just in case?)
    # moved this up here just to get the @project_archive.id set
    old = ProjectArchive.find(:first, :conditions => ['project_id = ? and archive_no = ?', @project.id, nextArchiveNo])
    old.destroy if old
    # add new projectArchive record
    project_archive = ProjectArchive.new
    project_archive.project_id = @project.id 
    project_archive.archive_no = nextArchiveNo
    project_archive.file_name = @filename
    project_archive.file_size = File.size(path_to_file)
    project_archive.file_date = Time.now    # TODO: add .utc to make UTC time?
    unless project_archive.save
      flash[:warning] += "error saving project_archive record for: #{@filename}"
      return false
    end

    @project.archive_count = nextArchiveNo

    unless @project.save
      flash[:warning] += "project record save failed"
      return false
    end

    unless expand_archive(project_archive)
      @project.archive_count = nextArchiveNo - 1
      @projects.status = "expand failed"
      @project.save
      return false
    end
    #redirect_to :action => 'show', :id => @project

    #@upload.methods.each {|x| msg += "#{x.to_str}<br>"}
    return true

  end

  def getErrText
    # get error output file
    @filename = "validate_error"
    errFile = path_to_file
    return File.open(errFile, "rb") { |f| f.read }
  rescue
    return ""
  end

  def getLoadErrText
    # get error output file
    @filename = "load_error"
    errFile = path_to_file
    return File.open(errFile, "rb") { |f| f.read }
  rescue
    return ""
  end

  def getUploadErrText
    # get error output file
    @filename = "upload_error"
    errFile = path_to_file
    return File.open(errFile, "rb") { |f| f.read }
  rescue
    return ""
  end

end
