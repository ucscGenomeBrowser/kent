class ClassifiedController < ApplicationController

  before_filter :login_required
  
  layout 'standard'
  
  def list
    @classifieds = Classified.find(:all)
  end
  
  def show_category
    @category = Category.find(params[:id])
  end
  
  def show
    @classified = Classified.find(params[:id])
  end

  def new
    @classified = Classified.new
    @categories = Category.find(:all)
  end
  
  def create
    @classified = Classified.new(params[:classified])
    if @classified.save
      redirect_to :action => 'list'
    else
      render :action => 'new'
    end
    @categories = Category.find(:all)
  end
  
  def edit
    @classified = Classified.find(params[:id])
    @categories = Category.find(:all)
  end
  
  def update
    @classified = Classified.find(params[:id])
    @categories = Category.find(:all)
    if @classified.update_attributes(params[:classified])
      redirect_to :action => 'show', :id => @classified
    else
      render :action => 'edit'
    end
  end
  
  def delete
    Classified.find(params[:id]).destroy
    redirect_to :action => 'list'
  end
  
end
