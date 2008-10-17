class PipelineController < ApplicationController

  # TODO: move some stuff into the private area so it can't be called by URL-hackers

  #CONSTANTS

  AUTOUPLOADLABEL = "Submit"

  before_filter :login_required
  before_filter :check_user_is_owner, :except => 
        [:new, :create, :list, :show_user, :show, :delete_archive, 
        :valid_status, :load_status, :unload_status, :upload_status ]
  
  layout 'main'
  
  def list
    @autoRefresh = true
    @projects = Project.find(:all, :order => 'name')
  end
  
  def show_user

    @autoRefresh = true
    @user = User.find(current_user.id)
    @projects = @user.projects

    render :action => 'list'
    
  end
  
  def show
    @autoRefresh = true
    @project = Project.find(params[:id])
    @errText = ""
    case @project.status
      when "validate failed"
        @errText = getErrText
      when "load failed"
        @errText = getLoadErrText
      when "unload failed"
	@errText = getUnloadErrText
      when "upload failed"
	@errText = getUploadErrText
      when "uploading"
        projectDir = path_to_project_dir
 	upText = getUploadErrText.split("\n")
        unless upText.blank?
 	  upText = upText.last.split(" ").first
          if upText.last(1) == "K"
 	    @uploadText = upText
          end
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

  def unload_status
    @project = Project.find(params[:id])
    @errText = getUnloadErrText
  end

  def upload_status
    @project = Project.find(params[:id])
    @errText = getUploadErrText
  end

  def begin_loading
    @project = Project.find(params[:id])
    if @project.status == "validated"
      @project.status = "loading"
      @project.save
      log_project_status
    end
    redirect_to :action => 'show', :id => @project.id
    galtDebug = false  # set to true to cause processing in parent without child 
                       # for debugging so you can see the error messages
    if galtDebug
      load
    else
      spawn do
        load 
      end
    end
  end

  def load
    projectType = getProjectType
 
    projectDir = path_to_project_dir
    cmd = "#{projectType['loader']} #{projectType['load_params']} #{projectDir} &> #{projectDir}/load_error"
    timeout = projectType['load_time_out']

    #logger.info "GALT! cmd=#{cmd} timeout=#{timeout}"

    exitCode = run_with_timeout(cmd, timeout)

    if exitCode == 0
      @project.status = "loaded"
      # send email notification
      if ActiveRecord::Base.configurations[RAILS_ENV]['emailOnLoad']
        user = User.find(current_user.id)
        UserNotifier.deliver_load_notification(user, @project)
      end
    else
      @project.status = "load failed"
    end
    @project.save
    log_project_status

  end 

  def begin_validating
    @project = Project.find(params[:id])
    if @project.status == "uploaded"
      @project.status = "validating"
      @project.save
      log_project_status
    end
    redirect_to :action => 'show', :id => @project.id
    spawn do
      validate      
    end
  end

  def validate
    projectType = getProjectType
 
    #require 'pp'   # debug
    #logger.info(projectType.pretty_inspect)

    projectDir = path_to_project_dir
    cmd = "#{projectType['validator']} #{projectType['type_params']} #{projectDir} &> #{projectDir}/validate_error"
    timeout = projectType['time_out']

    #logger.info "GALT! cmd=#{cmd} timeout=#{timeout}"

    exitCode = run_with_timeout(cmd, timeout)

    if exitCode == 0
      @project.status = "validated"
    else
      @project.status = "validate failed"
    end
    @project.save
    log_project_status

  end 

  def new
    @project = Project.new
    @projectTypes = getProjectTypes
    #@projectTypesArray = @projectTypes.to_a.sort_by { |x| x[1]["displayOrder"] }
  end
  
  def create
    if params[:commit] == "Cancel"
      redirect_to :action => 'show_user'
      return
    end
    @project = Project.new(params[:project])
    @project.user_id = @current_user.id 
    @project.status = 'new'
    @project.archives_active = ""
    if @project.save
      redirect_to :action => 'upload', :id => @project.id
      log_project_status
    else
      @projectTypes = getProjectTypes
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
        log_project_status
	unless @project.save
	  flash[:error] = "System error - project record save failed."
	  return false
	end
      end
      redirect_to :action => 'show', :id => @project
    else
      render :action => 'edit'
    end
  end
  
  def delete
    # call an unload cleanup routine 
    #  (e.g. that can remove .wib symlinks from /gbdb/ to the submission dir)
    projectDir= path_to_project_dir
    msg = ""
    msg += "Project deleted."
    if File.exists?(projectDir)
      @project.status = "unloading"
      unless @project.save
        flash[:error] = "System error - project record save failed."
        #@project.errors.each_full { |x| msg += x + "<br>" }
        redirect_to :action => 'show', :id => @project
        return
      end
    
      projectType = getProjectType
 
      projectDir = path_to_project_dir
      cmd = "#{projectType['unloader']} #{projectType['unload_params']} #{projectDir} &> #{projectDir}/unload_error"
      timeout = projectType['unload_time_out']

      #logger.info "GALT! cmd=#{cmd} timeout=#{timeout}"

      exitCode = run_with_timeout(cmd, timeout)

      if exitCode == 0
        @project.status = "unloaded"
        @project.save
        log_project_status
      else
        msg = "Project unload failed."
        flash[:notice] = msg
        @project.status = "unload failed"
        @project.save
        redirect_to :action => 'show', :id => @project
        return
      end

    end    
    delete_completion
    @project.status = "deleted"
    @project.save
    log_project_status
    unless @project.destroy
        @project.errors.each_full { |x| msg += x + "<br>" }
    end
    flash[:notice] = msg
    redirect_to :action => 'show_user'

  end
  
  def upload
    @project = Project.find(params[:id])
    @autoUploadLabel = AUTOUPLOADLABEL
    # handle FTP stuff
    @user = current_user
    @ftpUrl = "ftp://#{@user.login}@#{ActiveRecord::Base.configurations[RAILS_ENV]['ftpServer']}"+
           ":#{ActiveRecord::Base.configurations[RAILS_ENV]['ftpPort']}"
    @ftpList = []
    @fullPath = ActiveRecord::Base.configurations[RAILS_ENV]['ftpMount']+'/'+@user.login
    extensions = ["zip", "ZIP", "tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2", "tgz", "TGZ"]
    if File.exists?(@fullPath)
      Dir.entries(@fullPath).each do
        |file|
        fullName = File.join(@fullPath,file)
        if File.ftype(fullName) == "file"
          if extensions.any? {|ext| file.ends_with?("." + ext) }
            @ftpList << file
          end
        end
      end
    end

    return unless request.post?
    if params[:commit] == "Cancel"
      redirect_to :action => 'show', :id => @project
      return
    end
    @upurl = params[:upload_url]
    @upload = params[:upload_file]
    @upftp = params[:ftp]
    if @upurl == "http://"
      @upurl = ""
    end
    unless @upftp
      @upftp = ""
    end
    unless @upftp
      @upftp = ""
    end

    return if @upload.blank? && @upurl.blank? && @upftp.blank?

    unless @upurl.blank?
      @filename = sanitize_filename(@upurl)
      #TODO add a HEAD would check it exists before starting?
    else
      unless @upftp.blank?
        @filename = sanitize_filename(@upftp)
      else
        @filename = sanitize_filename(@upload.original_filename)
        extensionsByMIME = {
          "application/zip" => ["zip", "ZIP"],
          "application/x-compressed-tar" => ["tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2", "tgz", "TGZ"],
          "application/x-tar" => ["tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2", "tgz", "TGZ"],
          "application/octet-stream" => ["tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2", "tgz", "TGZ"],
          "application/gzip" => ["tar.gz", "TAR.GZ", "tgz", "TGZ"],
          "application/x-gzip" => ["tar.gz", "TAR.GZ", "tgz", "TGZ"]
        }
        extensions = extensionsByMIME[@upload.content_type.chomp]
        unless extensions
          flash[:error] = "Invalid content_type=#{@upload.content_type.chomp}."
          return
        end
      end
    end

    unless extensions.any? {|ext| @filename.ends_with?("." + ext) }
      flash[:error] = "File name <strong>#{@filename}</strong> is invalid. " +
        "Only a compressed archive file (tar.gz, tar.bz2, zip) is allowed."
      return
    end

    msg = ""

    # make sure parent paths exist
    projectDir = File.dirname(path_to_file)
    Dir.mkdir(projectDir,0775) unless File.exists?(projectDir)

    nextArchiveNo = @project.archive_count+1

    plainName = @filename
    @filename = "#{"%03d" % nextArchiveNo}_#{@filename}"

    #debugging
    #msg += "sanitized filename=#{@filename}<br>"
    #msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    #msg += "upload path=#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}<br>"
    #msg += "path_to_file=#{path_to_file}<br>"
    #msg += "nextArchiveNo=#{nextArchiveNo}<br>"

    msg += "Uploading/expanding #{plainName}.<br>"


    galtDebug = false  # set to true to cause processing in parent without child 
                       # for debugging so you can see the error messages

    if galtDebug
      upload_background
    end
     
    flash[:notice] = msg
    redirect_to :action => 'show', :id => @project

    unless galtDebug
      spawn do
        upload_background
      end
    end
 
  end

  def upload_background

      saveProjectStatus = @project.status
      @project.status = "uploading"
      unless @project.save
        return
      end
      log_project_status

      # just in case, remove it if it already exists (shouldn't happen)
      File.delete(path_to_file) if File.exists?(path_to_file)
    
      if @project.project_archives.last
        @project.project_archives.last.status = saveProjectStatus
        @project.project_archives.last.archives_active = @project.archives_active
        unless @project.project_archives.last.save
          flash[:error] = "System error - project_archive record status save failed."
          return
        end
      end

      unless @upurl.blank?

        projectDir = path_to_project_dir
        # trying for more info, remove -nv option:
        autoResume = @params['auto_resume']['0'] == "1" ? " -c" : ""
        cmd = "wget -O #{path_to_file}#{autoResume} '#{@upurl}' &> #{projectDir}/upload_error" 

	logger.info "\n\nGALT! autoResume=[#{autoResume}]\ncmd=[#{cmd}]\n\n"

        timeout = 36000  # 10 hours
        exitCode = run_with_timeout(cmd, timeout)
        if exitCode != 0
          @project.status = "upload failed"
          @project.save
          log_project_status
          return
        end

      else 
        unless @upftp.blank?
          FileUtils.copy(File.join(@fullPath,@upftp), path_to_file)
          File.delete(File.join(@fullPath,@upftp))
        else
          # should be local-file upload, Hmm... where does Mongrel put it during upload?
          unless defined? @upload.local_path
            FileUtils.copy(@upload.local_path, path_to_file)
          else
            File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
          end
        end
      end

      @project.status = "expanding"
      unless @project.save
        return
      end
      log_project_status

      nextArchiveNo = @project.archive_count+1
      if prep_one_archive @filename, nextArchiveNo
        if process_uploaded_archive
          @project.status = "uploaded"
        else
          @project.status = "upload failed"
        end
      else
          @project.status = "upload failed"
      end

      unless @project.save
        flash[:error] = "System error - project record save failed."
        return
      end
      log_project_status

      #if params[:commit] == @autoUploadLabel 
      validate
      if @project.status == "validated"
        @project.status = "loading"
        @project.save
        log_project_status
        load
      end
      #end
  end

  def delete_archive
    archive = ProjectArchive.find(params[:id])
    params[:id] = archive.project_id
    unless check_user_is_owner
      return false
    end
    msg = ""
    @project = Project.find(params[:id])
    n = archive.archive_no-1
    c = @project.archives_active[n..n]
    if c == "1"
      c = "0"
      msg += "Archive deleted." 
    else
      c = "1"
      msg += "Archive undeleted." 
    end
    @project.archives_active[n..n] = c

    unless @project.save
      flash[:error] = "System error - project record save failed."
      redirect_to :action => 'show', :id => @project
      return false
    end

    #old way: archive.destroy  #we do not delete, just mark
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
    keepers["upload_error"] = "keep"
    keepers["out"] = "keep"

    msg = ""
    # make sure parent paths exist
    projectDir = path_to_file

    # clean out directory
    Dir.entries(projectDir).each do 
      |f| 
      fullName = File.join(projectDir,f)
      unless keepers[f] or (f == ".") or (f == "..")
        cmd = "rm -fr #{fullName}"
        unless system(cmd)
          flash[:error] = "System error cleaning up subdirectory: <br>command=[#{cmd}].<br>"  
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

    found = false 
    @project.project_archives.each do |a|
      n = a.archive_no-1
      c = @project.archives_active[n..n]
      if c == "1"
        found = true
      end
    end

    unless found 
      @project.status = "new"
      msg = "All archives are currently marked as deleted."
    else
      @project.status = "re-expanding all"
      msg = "Cleaning out upload dir and re-expanding all archives."
    end
    unless @project.save
      flash[:error] = "System error - project record save failed."
      redirect_to :action => 'show', :id => @project
      return false
    end   
    log_project_status

    if flash[:notice]
      flash[:notice] += "<br>"+msg
    else
      flash[:notice] = msg
    end 
    redirect_to :action => 'show', :id => @project
    if found
      spawn do
        @project.project_archives.each do |a|
          n = a.archive_no-1
          c = @project.archives_active[n..n]
          if c == "1"
            prep_one_archive a.file_name, a.archive_no
          end
        end
        reexpand_all_completion 
      end
    end

  end

  # --------- PRIVATE ---------
private
  
  def check_user_is_owner
    @project = Project.find(params[:id])
    unless @project.user_id == @current_user.id
      flash[:error] = "That project does not belong to you."
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

  def path_to_project_dir
    # the expand_path method resolves this relative path to full absolute path
    File.expand_path("#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}/#{@project.id}")
  end


  def path_to_file
    # the expand_path method resolves this relative path to full absolute path
    path_to_project_dir+"/#{@filename}"
  end

  #TODO : turn projectTypes back into a yaml file in config dir

  # --- read project types from config file into hash -------
  def getProjectTypes
    #open("#{RAILS_ROOT}/config/projectTypes.yml") { |f| YAML.load(f.read) }
    ProjectType.find(:all, :conditions => ['display_order != 0'], :order => "display_order")
  end

  # --- read one project type from config file into hash -------
  def getProjectType
    projectTypes = getProjectTypes
    projectTypes.each do |x|
      if x['id'] == @project.project_type_id
        return x
      end
    end
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


  def clean_out_dir(path)
    if File.exists?(path)
      cmd = "rm -fr #{path}"
      unless system(cmd)
        flash[:error] = "System error cleaning out subdirectory: <br>command=[#{cmd}].<br>"  
        return false
      end
    end
    return true
  end

  def prep_one_archive(file_name, archive_no)

    # make sure parent paths exist
    projectDir = path_to_project_dir

    # make a temporary upload directory to unpack and merge from
    uploadDir = projectDir+"/upload_#{archive_no}"
    clean_out_dir uploadDir
    Dir.mkdir(uploadDir,0775)

    @filename = file_name
    cmd = makeUnarchiveCommand(uploadDir)
    timeout = 3600
    exitCode = run_with_timeout(cmd, timeout)

    #debug
    #logger.info "got past running decompressor:\ncmd=[#{cmd}]\nexitCode=#{exitCode}\n"

    if exitCode != 0
      return false
    end
    return true

  end


  def expand_archive(archive)
    # expand archive belonging to archive record

    projectDir = path_to_project_dir
    uploadDir = projectDir+"/upload_#{archive.archive_no}"

    @filename = archive.file_name

    process_archive(archive.id, projectDir, uploadDir, "")

    # cleanup: delete temporary upload subdirectory
    clean_out_dir uploadDir

    if @project.project_archives.length == 0
      @project.status = "new"
    else
      @project.status = "uploaded"
    end

    unless @project.save
      flash[:error] = "System error - project record save failed."
      return false
    end
    log_project_status

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
    fullPath = my_join(uploadDir,relativePath)
    Dir.entries(fullPath).each do
      |f| 
      fullName = my_join(fullPath,f)
      if (File.ftype(fullName) == "directory")
        if (f != ".") and (f != "..")
          newRelativePath = my_join(relativePath,f)
          newDir = my_join(projectDir, newRelativePath)
	  unless File.exists?(newDir)
	    Dir.mkdir(newDir,0775)
	  end
          process_archive(archive_id, projectDir, uploadDir, newRelativePath)
        end
      else 
        if File.ftype(fullName) == "file"
   
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
            flash[:error] = "System error saving project_file record for: #{f}."
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
    
    logger.info "\n\n@filename=#{@filename}\n\n" #debug

    # make sure parent paths exist
    projectDir = path_to_project_dir

    nextArchiveNo = @project.archive_count+1

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
    project_archive.status = "see current"
    project_archive.archives_active = ""
    unless project_archive.save
      return false
    end

    @project.archive_count = nextArchiveNo
    @project.archives_active += "1"

    unless @project.save
      return false
    end

    unless expand_archive(project_archive)
      @project.archive_count = nextArchiveNo - 1
      @project.status = "expand failed"
      @project.save
      log_project_status
      return false
    end

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

  def getUnloadErrText
    # get error output file
    @filename = "unload_error"
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

  def makeUnarchiveCommand(uploadDir)
    # handle unzipping the archive
    if ["zip", "ZIP"].any? {|ext| @filename.ends_with?("." + ext) }
      cmd = "unzip -o  #{path_to_file} -d #{uploadDir} &> #{File.dirname(uploadDir)}/upload_error"   # .zip 
    else
      if ["gz", "GZ", "tgz", "TGZ"].any? {|ext| @filename.ends_with?("." + ext) }
        cmd = "tar -xzf #{path_to_file} -C #{uploadDir} &> #{File.dirname(uploadDir)}/upload_error"  # .gz .tgz gzip 
      else  
        cmd = "tar -xjf #{path_to_file} -C #{uploadDir} &> #{File.dirname(uploadDir)}/upload_error"  # .bz2 bzip2
      end
    end
  end

  def reexpand_all_completion  
    # after background expansion of all archives not marked as deleted

    @project.project_archives.each do |a|
      n = a.archive_no-1
      c = @project.archives_active[n..n]
      if c == "1"
        unless expand_archive(a)
          return
        end
      end
    end

  end

  def delete_completion

    projectDir= path_to_project_dir
    if File.exists?(projectDir)
      Dir.entries(projectDir).each { 
        |f| 
        unless (f == ".") or (f == "..")
          fullName = File.join(projectDir,f)
          cmd = "rm -fr #{fullName}"
          unless system(cmd)
            flash[:error] = "System error cleaning out project subdirectory: <br>command=[#{cmd}].<br>"  
	    redirect_to :action => 'show_user'
            return
          end
        end
      }
      Dir.delete(projectDir)
    end
  end


  def log_project_status
    # add new projectArchive record
    project_status_log = ProjectStatusLog.new
    project_status_log.project_id = @project.id 
    project_status_log.status = @project.status
    unless project_status_log.save
      flash[:error] = "System error saving project_status_log record."
    end
  end
 
end
