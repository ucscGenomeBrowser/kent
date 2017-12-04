# A program that will attempt to make a list of fields out of a json schema that may refer
# to other schemas in the same directory.
import os
import json

def recurse_emit_node_fields(dir, file, parent):
   path = os.path.join(dir, file)
   with open(path) as json_data:
        schema = json.load(json_data)
        properties = schema["properties"]
        for field in properties.keys():
            info = properties[field]
            if "type" in info:
                type = info["type"]
                if type == "array":
                    field = field + ".[]"
                    info = info["items"]
            if "$ref" in info:
                ref = info["$ref"]
                if ref[0] == '#':
                    print "%s.%s # Needs unpacking" % (parent, field)
                else:
                    sub_file = os.path.split(ref)[-1]
                    sub_parent = "%s.%s" % (parent, field)
                    recurse_emit_node_fields(dir, sub_file, sub_parent)
            else:
                print "%s.%s" % (parent,field)

entities = [ "project", "assay", "sample" ]
for entity in entities:
   recurse_emit_node_fields("v4/json_schema", entity + ".json", entity)


