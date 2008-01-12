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
    if @project.status.starts_with?("schedule expand all")
      reexpand_all_completion
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
    redirect_to :action => 'show_user'
  end

  def begin_validating
    @project = Project.find(params[:id])
    if @project.status == "uploaded"
      @project.status = "schedule validating"
      @project.save
    end
    redirect_to :action => 'show_user'
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
    @project.archives_active = ""
    if @project.save
      redirect_to :action => 'show', :id => @project.id
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
    projectDir= path_to_project_dir
    if File.exists?(projectDir)
      Dir.entries(projectDir).each { 
        |f| 
        unless (f == ".") or (f == "..")
          fullName = File.join(projectDir,f)
          cmd = "rm -fr #{fullName}"
          exitCode = system(cmd)
          unless exitCode 
            flash[:warning] = "error cleaning up temporary upload subdirectory: <br>command=[#{cmd}], exitCode = #{exitCode}<br>"  
	    redirect_to :action => 'show_user'
            return
          end
        end
      }
      Dir.delete(projectDir)
    end
    Project.find(params[:id]).destroy
    redirect_to :action => 'show_user'
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
      #old way not used: @upload = open(@upurl)
    else
      @filename = sanitize_filename(@upload.original_filename)
      extensionsByMIME = {
        "application/zip" => ["zip", "ZIP"],
        "application/x-tar" => ["tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2"],
        "application/gzip" => ["tar.gz", "TAR.GZ"]
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
    Dir.mkdir(projectDir,0775) unless File.exists?(projectDir)

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
    
    if @project.project_archives.last
      @project.project_archives.last.status = @project.status
      @project.project_archives.last.archives_active = @project.archives_active
      unless @project.project_archives.last.save
        flash[:warning] = "project_archive record status save failed"
        return
      end
    end

    initiateToDo

    unless @upurl.blank?
      toDo "wget -nv -O #{path_to_file} '#{@upurl}'"
      #@project.status = "schedule todo (upload and expand)"
    else
      if @upload.instance_of?(Tempfile)
        FileUtils.copy(@upload.local_path, path_to_file)
      else
        File.open(path_to_file, "wb") { |f| f.write(@upload.read) }
      end
      #@project.status = "schedule todo (expand)"
    end
    @project.status = "schedule uploading #{plainName}"

    unless @project.save
      flash[:warning] = "project record save failed"
      return
    end

    prep_one_archive @filename, nextArchiveNo

    flash[:notice] = msg
    redirect_to :action => 'show', :id => @project

  end



  def delete_archive
    archive = ProjectArchive.find(params[:id])
    params[:id] = archive.project_id
    unless check_user_is_owner
      redirect_to :action => 'show', :id => @project
      return
    end
    msg = ""
    @project = Project.find(params[:id])
    n = archive.archive_no-1
    c = @project.archives_active[n..n]
    if c == "1"
      c = "0"
      msg += "Archive deleted" 
    else
      c = "1"
      msg += "Archive undeleted" 
    end
    @project.archives_active[n..n] = c

    unless @project.save
      flash[:warning] = "project record save failed"
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

    msg = ""
    # make sure parent paths exist
    projectDir = path_to_file

    # clean out directory
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

    initiateToDo
    found = false 
    @project.project_archives.each do |a|
      n = a.archive_no-1
      c = @project.archives_active[n..n]
      if c == "1"
        found = true
        prep_one_archive a.file_name, a.archive_no
      end
    end

    unless found 
      @project.status = "new"
      msg = "All archives are currently marked as deleted"
    else
      @project.status = "schedule re-expand all"
      msg = "Scheduling clean out of upload dir and re-expansion of all archives"
    end
    unless @project.save
      flash[:warning] = "project record save failed"
      redirect_to :action => 'show', :id => @project
      return false
    end

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
      redirect_to :action => 'show_user'
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


  def clean_out_dir(path)
    if File.exists?(path)
      cmd = "rm -fr #{path}"
      exitCode = system(cmd)
      unless exitCode 
        flash[:warning] = "command=[#{cmd}], exitCode = #{exitCode}<br>"  
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
    toDo cmd

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
	    Dir.mkdir(newDir,0775)
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
    projectDir = path_to_project_dir

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
    project_archive.status = "see current"
    project_archive.archives_active = ""
    unless project_archive.save
      flash[:warning] = "error saving project_archive record for: #{@filename}"
      return false
    end

    @project.archive_count = nextArchiveNo
    @project.archives_active += "1"

    unless @project.save
      flash[:warning] = "project record save failed"
      return false
    end

    unless expand_archive(project_archive)
      @project.archive_count = nextArchiveNo - 1
      @project.status = "expand failed"
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

  def todoName
    # the expand_path method resolves this relative path to full absolute path
    path_to_project_dir+"/todo"
  end
  def toDo(line)
    f = File.open(todoName, "a")
    f.puts(line)
    f.close
  end
  def initiateToDo
    f = File.open(todoName, "w")
    f.close
    toDo "#!/bin/tcsh"
    File.chmod 0755, todoName
  end

  def makeUnarchiveCommand(uploadDir)
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
  end

  def reexpand_all_completion
    # after background expansion of all archives not marked as deleted

    @project.project_archives.each do |a|
      n = a.archive_no-1
      c = @project.archives_active[n..n]
      if c == "1"
        unless expand_archive(a)
	  redirect_to :action => 'show', :id => @project
          return
        end
      end
    end

    msg = "Cleaned out upload dir and re-expanded all archives"
    if flash[:notice]
      flash[:notice] += "<br>"+msg
    else
      flash[:notice] = msg
    end 
    redirect_to :action => 'show', :id => @project

  end

end
