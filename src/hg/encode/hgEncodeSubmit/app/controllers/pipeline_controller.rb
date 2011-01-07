class PipelineController < ApplicationController

  include PipelineBackground
  include SortHelper

  # TODO: move some stuff into the private area so it can't be called by URL-hackers

  #CONSTANTS

  AUTOUPLOADLABEL = "Submit"

  helper :sort

  before_filter :login_required
  before_filter :check_user_is_owner, :except => 
        [ :new, :create, :list, :show_active, :show_user, :show, 
        :valid_status, :load_status, :unload_status, :upload_status, 
        :show_tools, :mass_tools, :mass_ftp, :mass_url, :delete_archive ]
  
  layout 'main'
  
  def list
    sort_init 'updated_at', {:default_order => 'desc'}
    sort_update
    #@autoRefresh = true
    @sort_key = params[:sort_key]
    if @sort_key == "pi" || @sort_key == "login"
      @projects = Project.find(:all, :include => :user, :order => sort_clause)
    elsif @sort_key
      @projects = Project.find(:all, :order => sort_clause)
    else
      @projects = Project.find(:all, :order => 'updated_at DESC')
    end
  end
  
  def show_active
    sort_init 'updated_at', {:default_order => 'desc'}
    sort_update
    #@autoRefresh = true
    active_conditions = [
      "status != 'reviewing'  and " +
      "status != 'displayed'  and " +
      "status != 'approved'   and " +
      "status != 'revoked'    and " +
      "status != 'released'"
    ]
    @sort_key = params[:sort_key]
    if @sort_key == "pi" || @sort_key == "login"
      @projects = Project.find(:all, :include => :user, :order => sort_clause, :conditions => active_conditions)
    elsif @sort_key
      @projects = Project.find(:all, :order => sort_clause, :conditions => active_conditions)
    else
      @projects = Project.find(:all, :order => 'updated_at DESC', :conditions => active_conditions)
    end
    render :action => 'list'    
  end
  
  def show_user
    sort_init 'updated_at', {:default_order => 'desc'}
    sort_update
    @sort_key = params[:sort_key]    
    @user = params[:submitter] ? User.find(:first, :conditions => {:login => params[:submitter]}) : User.find(current_user.id)
    if @sort_key
      @projects = Project.find(:all, :conditions => {:user_id => @user.id}, :order => sort_clause)
    else 
      @projects = Project.find(:all, :conditions => {:user_id => @user.id}, :order => 'updated_at DESC')
    end
    @projects.each do 
      |p|
      if p.run_stat
        @autoRefresh = true
      end
    end
    render :action => 'list'    
  end

  def show
    @project = Project.find(params[:id])
    if @project.run_stat
      @autoRefresh = true
    end
    @errText = ""
    case @project.status
      when "upload failed"
	@errText = getUploadErrText(@project)
      when "expand failed"
	@errText = getExpandErrText(@project)
      when "validate failed"
        @errText = getValidateErrText(@project)
      when "load failed"
        @errText = getLoadErrText(@project)
      when "unload failed"
	@errText = getUnloadErrText(@project)
      when "uploading"
	# old wget method
	#upText = getUploadErrText(@project)
        #unless upText.blank?
	#  upText = upText.split("\n")
	#  if upText.last
	#    upText = upText.last.split(" ").first
	#    if upText.last(1) == "K"
	#      @uploadText = upText
	#    end
        #  end
        #end

	# new paraFetch method
	upText = getNewestFileByExtensionIgnoringCase(@project.id, "paraFetchStatus")

        unless upText.blank?
	  upText = upText.split("\n")
	  unless upText.length < 4
	    url = upText.shift
	    size = upText.shift.to_i
	    fdate = upText.shift
	    downloaded = 0
	    while not upText.empty?
	      l = upText.shift
	      downloaded += l.split(" ").last.to_i
	    end
	    @uploadText = "#{goGreek(downloaded)} of #{goGreek(size)} #{url}"
	  end
        end

    end 
    @dafName,@dafText = getDafText(@project)
    @ddfName,@ddfText = getDdfText(@project)

    if @project.run_stat and @project.run_stat == "waiting"
      job = QueuedJob.find(:first, :conditions => ["project_id = ?", @project.id])
      if job
        @aheadOfYou = QueuedJob.count_by_sql "select count(*) from queued_jobs where id < #{job.id} and source_code <> 'done'"
      else
        @aheadOfYou = nil
      end
    end

    @deadline = get_deadline

  rescue
    redirect_to :action => 'show_user'
    return
  end


  def valid_status
    @project = Project.find(params[:id])
    @errText = getValidateErrText(@project)
  end

  def load_status
    @project = Project.find(params[:id])
    @errText = getLoadErrText(@project)
  end

  def unload_status
    @project = Project.find(params[:id])
    @errText = getUnloadErrText(@project)
  end

  def upload_status
    @project = Project.find(params[:id])
    @errText = getUploadErrText(@project)
  end

  def expand_status
    @project = Project.find(params[:id])
    @errText = getExpandErrText(@project)
  end

  def show_daf
    @project = Project.find(params[:id])
    @dafName,@dafText = getDafText(@project)
  end

  def download_daf
    @project = Project.find(params[:id])
    @dafName,@dafText = getDafText(@project)
    headers.merge!('Content-Disposition' => "attachment; filename=\"#{@dafName}\"")
    render :text => @dafText, :content_type => 'text/plain'
  end

  def show_ddf
    @project = Project.find(params[:id])
    @ddfName,@ddfText = getDdfText(@project)
  end

  def download_ddf
    @project = Project.find(params[:id])
    @ddfName,@ddfText = getDdfText(@project)
    headers.merge!('Content-Disposition' => "attachment; filename=\"#{@ddfName}\"")
    render :text => @ddfText, :content_type => 'text/plain'
  end

  def db_load
    @project = Project.find(params[:id])
    if @project.run_stat 
      flash[:error] = "Please wait, a background job is still running."
      redirect_to :action => 'show', :id => @project.id
      return
    end
    if @project.status == "validated"
      new_status @project, "load requested"
      unless queue_job @project.id, "load_background(#{@project.id})"
        flash[:error] = "System error - queued_jobs save failed."
        return
      end
    end
    redirect_to :action => 'show', :id => @project.id
  end 

  def validate
    @project = Project.find(params[:id])
    allowReloads = ""
    if @project.run_stat 
      flash[:error] = "Please wait, a background job is still running."
      redirect_to :action => 'show', :id => @project.id
      return
    end
    if (@project.status == "expanded") or (@project.status == "validate failed")
      new_status @project, "validate requested"
      unless queue_job @project.id, "validate_background(#{@project.id}, \"#{allowReloads}\")"
        flash[:error] = "System error - queued_jobs save failed."
        return
      end
    end
    redirect_to :action => 'show', :id => @project.id
  end 

  def new
    @project = Project.new
    @projectTypes = getProjectTypes
    @deadline = get_deadline
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
      log_project_status @project
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
	  @project.status = "expanded"
	end
        new_status @project, @project.status
      end
      redirect_to :action => 'show', :id => @project
    else
      render :action => 'edit'
    end
  end
  
  def unload
    @project = Project.find(params[:id])
    if @project.run_stat 
      flash[:error] = "Please wait, a background job is still running."
      redirect_to :action => 'show', :id => @project.id
      return
    end
    projectDir= path_to_project_dir(@project.id)
    msg = ""
    msg += "Submission unload requested."
    new_status @project, "unload requested"
    unless queue_job @project.id, "unload_background(#{@project.id})"
      flash[:error] = "System error - queued_jobs save failed."
      return
    end
    flash[:notice] = msg
    redirect_to :action => 'show', :id => @project
  end

  def delete
    @project = Project.find(params[:id])
    if @project.run_stat 
      flash[:error] = "Please wait, a background job is still running."
      redirect_to :action => 'show', :id => @project.id
      return
    end
    projectDir= path_to_project_dir(@project.id)
    msg = ""
    msg += "Submission delete requested."
    new_status @project, "delete requested"
    unless queue_job @project.id, "delete_background(#{@project.id})"
      flash[:error] = "System error - queued_jobs save failed."
      return
    end
    flash[:notice] = msg
    redirect_to :action => 'show_user'
  end




  def mass_ftp
    @user = current_user

    get_ftp_list

    return unless request.post?
    if params[:commit] == "Cancel"
      redirect_to :action => 'show_user'
      return
    end

    # create submissions
    @ftpList.each do |file|
      #take project name from archive filename
      project_name = file
      #chop off archive file extension
      @extensions.each do |ext|
        if project_name.ends_with?("." + ext)
          project_name = project_name[0..(project_name.length - 1 - (ext.length + 1))]
        end    
      end
      #if a submission with that name exists already, tweak the name with (number) until unique.
      base_name = project_name
      suffix = ""
      retryCount = 0
      unique = false
      while not unique
        project_name = base_name + suffix
        @existing = Project.find(:first, :conditions => {:name => project_name})
        if @existing
          retryCount += 1
          suffix = " (" + retryCount.to_s + ")"
        else
          unique = true
        end
      end

      @project = Project.new()
      @project.name = project_name
      @project.project_type_id = 5
      @project.user_id = @current_user.id
      @project.status = 'new'
      @project.archives_active = ""

      if @project.save
        # queue it up to be uploaded etc.
        @upurl = ""
        @upload = ""
        @upftp = file
        @filename = sanitize_filename(@upftp)
        upload_one
      else
        msg = ("Error creating submission for "+file+" as "+project_name+"<br>")
        if flash[:error]
          flash[:error] += msg
        else
          flash[:error] = msg
       end
      end

    end

    redirect_to :action => 'show_user'

  end


  def mass_url
    @user = current_user

    return unless request.post?
    if params[:commit] == "Cancel"
      redirect_to :action => 'show_user'
      return
    end

    uplist = params[:url_list].split("\n")

    
    @extensions = ["zip", "ZIP", "tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2", "tgz", "TGZ"]
    # check urls have correct extension
    uplist.each do |url|
      # get trim lead/trail whitespace
      url.strip!
      if url.blank? 
        next
      end
      @filename = sanitize_filename(url)
      # todo may need to move this up earlier?
      unless @extensions.any? {|ext| @filename.ends_with?("." + ext) }
        flash[:error] = "File name <strong>#{@filename}</strong> is invalid. " +
          "Only a compressed archive file (tar.gz, tar.bz2, zip) is allowed."
        return
      end
    end

    # create submissions
    uplist.each do |url|
      # get trim lead/trail whitespace
      url.strip!
      if url.blank? 
        next
      end
      #take project name from archive filename
      project_name = url
      #chop off archive file extension
      @extensions.each do |ext|
        if project_name.ends_with?("." + ext)
          project_name = project_name[0..(project_name.length - 1 - (ext.length + 1))]
        end    
      end
      #if a submission with that name exists already, tweak the name with (number) until unique.
      base_name = project_name
      suffix = ""
      retryCount = 0
      unique = false
      while not unique
        project_name = base_name + suffix
        @existing = Project.find(:first, :conditions => {:name => project_name})
        if @existing
          retryCount += 1
          suffix = " (" + retryCount.to_s + ")"
        else
          unique = true
        end
      end

      @project = Project.new()
      @project.name = project_name
      @project.project_type_id = 5
      @project.user_id = @current_user.id
      @project.status = 'new'
      @project.archives_active = ""

      if @project.save
        # queue it up to be uploaded etc.
        @upurl = url
        @upload = ""
        @upftp = ""
        @filename = sanitize_filename(@upurl)
        upload_one
      else
        msg = ("Error creating submission for "+url+" as "+project_name+"<br>")
        if flash[:error]
          flash[:error] += msg
        else
          flash[:error] = msg
       end
      end

    end

    redirect_to :action => 'show_user'

  end


  def upload
    @project = Project.find(params[:id])
    if @project.run_stat 
      flash[:error] = "Please wait, a background job is still running."
      redirect_to :action => 'show', :id => @project.id
      return
    end
    @user = current_user
    @autoUploadLabel = AUTOUPLOADLABEL

    get_ftp_list

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
        @extensions = extensionsByMIME[@upload.content_type.chomp]
        unless @extensions
          flash[:error] = "Invalid content_type=#{@upload.content_type.chomp}."
          return
        end
      end
    end

    unless @extensions.any? {|ext| @filename.ends_with?("." + ext) }
      flash[:error] = "File name <strong>#{@filename}</strong> is invalid. " +
        "Only a compressed archive file (tar.gz, tar.bz2, zip) is allowed."
      return
    end

    upload_one
    redirect_to :action => 'show', :id => @project

  end





  def delete_archive

    # This is really a toggle on/off and the archives are only marked for exclusion
    archive = ProjectArchive.find(params[:id])
    params[:id] = archive.project_id
    unless check_user_is_owner
      return false
    end
    msg = ""
    @project = Project.find(params[:id])
    if @project.run_stat 
      flash[:error] = "Please wait, a background job is still running."
      redirect_to :action => 'show', :id => @project.id
      return
    end
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

    flash[:notice] = msg

    new_status @project, "re-expand all requested"
    unless queue_job @project.id, "reexpand_all_background(#{@project.id})"
      flash[:error] = "System error - queued_jobs save failed."
      return
    end
    redirect_to :action => 'show', :id => @project.id
  end

  def reexpand_all
    @project = Project.find(params[:id])
    if @project.run_stat 
      flash[:error] = "Please wait, a background job is still running."
      redirect_to :action => 'show', :id => @project.id
      return
    end
    if @project.status != "new"
      new_status @project, "re-expand all requested"
      unless queue_job @project.id, "reexpand_all_background(#{@project.id})"
        flash[:error] = "System error - queued_jobs save failed."
        return
      end
      flash[:notice] = "re-expanding all"
    end
    redirect_to :action => 'show', :id => @project.id
  end 

  # --------- PRIVATE ---------
private
  
  def check_user_is_owner
    @project = Project.find(params[:id])
    unless @project.user_id == @current_user.id or @current_user.role == "admin"
      flash[:error] = "That project does not belong to you."
      redirect_to :action => 'list'
      return false
    end
    return true
  end

  def get_ftp_list
    @ftpUrl = "ftp://#{@user.login}@#{ActiveRecord::Base.configurations[RAILS_ENV]['ftpServer']}"+
           ":#{ActiveRecord::Base.configurations[RAILS_ENV]['ftpPort']}"
    @ftpList = []
    @fullPath = ActiveRecord::Base.configurations[RAILS_ENV]['ftpMount']+'/'+@user.login
    @extensions = ["zip", "ZIP", "tar.gz", "TAR.GZ", "tar.bz2", "TAR.BZ2", "tgz", "TGZ"]
    if File.exists?(@fullPath)
      Dir.entries(@fullPath).each do
        |file|
        fullName = File.join(@fullPath,file)
        if File.ftype(fullName) == "file"
          if @extensions.any? {|ext| file.ends_with?("." + ext) }
            @ftpList << file
          end
        end
      end
    end
    @ftpList.sort!
  end

  def upload_one

    bg_local_path = ""

    msg = ""

    # make sure parent paths exist
    projectDir = path_to_project_dir(@project.id)
    Dir.mkdir(projectDir,0775) unless File.exists?(projectDir)

    nextArchiveNo = @project.archive_count+1

    plainName = @filename
    @filename = "#{"%03d" % nextArchiveNo}_#{@filename}"

    #debugging
    #msg += "sanitized filename=#{@filename}<br>"
    #msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    #msg += "upload path=#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}<br>"
    #msg += "path_to_file=#{pf}<br>"
    #msg += "nextArchiveNo=#{nextArchiveNo}<br>"

    msg += "Uploading/expanding #{plainName}.<br>"

    # local file upload by browser cannot be passed in bg job params
    if @upurl.blank?
      if @upftp.blank?
        # should be local-file upload, Hmm... where does Mongrel put it during upload?
        pf = path_to_file(@project.id, @filename)
        if @upload.respond_to?(:local_path) and @upload.local_path and File.exists?(@upload.local_path)

          logger.info "#DEBUG local_path=#{@upload.local_path} length=#{@upload.length} original_filename=#{@upload.original_filename}"  #debug
          bg_local_path = "#{@upload.local_path}_BG"
          File.link(@upload.local_path, bg_local_path)  # we want the file to be preserved beyond cgi call for bg to use

        elsif @upload.respond_to?(:read)

          logger.info "#DEBUG length=#{@upload.length} original_filename=#{@upload.original_filename}"  #debug
          File.open(pf, "wb") { |f| f.write(@upload.read); f.close }

        else

          raise ArgumentError.new("Do not know how to handle #{@upload.inspect}?")

       end 
       @upload.close

       # old way
       #if defined? @upload.local_path
       #   FileUtils.copy(@upload.local_path, pf)
       # else
       #   File.open(pf, "wb") { |f| f.write(@upload.read) }
       # end

      end
    end
    allowReloads = "";
    if (defined? @params['allow_reloads']) and (@params['allow_reloads']['0'] == "1")
       allowReloads = "-allowReloads"
    end

    # need to preserve the status for the archive NOW
    if @project.project_archives.last
      @project.project_archives.last.status = @project.status
      @project.project_archives.last.archives_active = @project.archives_active
      unless saver @project.project_archives.last
        flash[:error] = "unable to save @project.project_archives.last" 
      end
    end

    new_status @project, "upload requested"
    upload_name = @upload.blank? ? "" : @upload.original_filename
    param_string = "#{@project.id},\"#{@upurl}\", \"#{@upftp}\", \"#{upload_name}\", \"#{bg_local_path}\", \"#{allowReloads}\""
    unless queue_job @project.id, "upload_background(#{param_string})"
      flash[:error] = "System error - queued_jobs save failed."
      return
    end

    if flash[:notice]
      flash[:notice] += msg
    else
      flash[:notice] = msg
    end

  end

end
