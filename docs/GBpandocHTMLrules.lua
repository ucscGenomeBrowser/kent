-- Function to create an HTML image tag with a given src and width
function add_image(src, width)
  -- Ensure src and width are provided
  if src == nil or width == nil then
    error("Error: src or width is nil in add_image")
  end
  return string.format('<img src="%s" width="%s">', src, width)
end

-- Function to detect and process code blocks with class "image"
function CodeBlock(el)
  -- Check if the code block is tagged with the class "image"
  if el.classes[1] == "image" then
    -- Extract src and width from the code block text
    local src = el.text:match("src=(%S+)")
    local width = el.text:match("width=(%S+)")
    
    -- Ensure that both src and width are captured
    if src and width then
      return pandoc.RawBlock("html", add_image(src, width))
    else
      error("Error: src or width not found in block")
    end
  end
  return el  -- Return the unchanged element if it's not an image block
end
