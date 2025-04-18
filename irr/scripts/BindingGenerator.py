import xml.etree.ElementTree as ET
import sys
from typing import List, Dict, Set, Optional, Tuple
from string import Template

# Python program for parse gl.xml into pyhton data containers (in GLinfo class) and generate header and cpp loader from templates.

class GLInfo:
	def __init__(self):
		self.__typedefs_db: List(str) = []
		self.__functions_db: Dict[str, Dict[str, Optional[Tuple[int, int]]]] = {}
		self.__functions_order: List(str) = []
		self.__enums_db: Dict[str, Dict[str]] = {}
		self.__enums_order: List(str) = []
		
		self.__allowed_extensions = ["GL_ARB_ES2_compatibility", "GL_ARB_framebuffer_object", "GL_ARB_imaging", "GL_ARB_texture_multisample", "GL_ARB_texture_storage_multisample", "GL_KHR_debug"]
		self.__ignore_api = ["GL_VERSION_ES_CM_1_0"]
		self.__unwanted_suffixes = ["_ANGLE", "ANGLE", "_APPLE", "APPLE", "_ARB", "ARB", "_EXT", "EXT", "_KHR", "KHR", "_NV", "NV"]

	def __parse_version(self, version: str) -> Tuple[int, int]:
		major, minor = map(int, version.split('.'))
		return major, minor

	def __is_wanted_extension(self, extension_name):
		# Check if the extension name contains any of the specified vendor keywords
		for keyword in self.__allowed_extensions:
			if keyword in extension_name:
				return True
		return False

	def __is_wanted_api(self, feature_name):
		# Check if the extension name contains any of the specified vendor keywords
		for keyword in self.__ignore_api:
			if keyword in feature_name:
				return False
		return True
	
	def __remove_suffix(self, text):
		for suffix in self.__unwanted_suffixes:
			if text.endswith(suffix):
				return text[:len(text) - len(suffix)]
		return text
	
	def __load_typedef(self, typedef):
		if not typedef.get('name'):
			type_text = typedef.text or ''
			for defpart in typedef:
				if (defpart.tag == 'apientry'):
					type_text += "APIENTRY"
				if defpart.text:
					type_text += defpart.text
				if defpart.tail:
					type_text += defpart.tail
			self.__typedefs_db.append(type_text)

	def __load_command(self, command):
		# Load a command element into the functions database
		proto = command.find('proto')
		func_name = proto.find('name').text
		func_alias = None
		if command.find('alias') is not None:
			func_alias = command.find('alias').get('name')

		if func_name not in self.__functions_db:
			parameters = ""
			separator = ''
			for param in command.findall('param'):
				parameters = "{}{}{}".format(parameters, separator, ''.join([text for text in param.itertext()]))
				separator = ", "
			if parameters=="":
				parameters = "void"

			ret_type = [text for text in proto.itertext()]
			ret_type = ''.join(ret_type[:-1])
			ret_type = ret_type.strip()

			clean_name = self.__remove_suffix(func_name)
			if (func_alias == None):
				clean_name = func_name

			self.__functions_db[func_name] = {
				'name': func_name,
				'name_clean': clean_name,
				'min_gl_version': None,
				'min_gles_version': None,
				'defined_extensions': set(),
				'aliases': set(),
				'have_alias': (func_alias != None),
				'return': ret_type,
				'parameters': parameters
			}

	def __load_alias(self, command):
		# process command alias
		func_name = command.find('proto').find('name').text
		func_alias = None
		if command.find('alias') is not None:
			func_alias = command.find('alias').get('name')
		if func_alias:
			self.__functions_db[func_alias]['aliases'].add(func_name)

	def __load_enum(self, enum):
		# load enum definition
		enum_name = enum.get('name')
		enum_alias = None
		if enum.get('alias') is not None:
			enum_alias = enum_name
			enum_name = enum.get('alias')

		clean_name = self.__remove_suffix(enum_name)
		if (enum_alias == None):
			if clean_name in self.__enums_db and self.__enums_db[clean_name]['value']==enum.get('value'):
				enum_alias = clean_name
			else:
				clean_name = enum_name
		
		if enum_name not in self.__enums_db:
			self.__enums_db[enum_name] = {
				'name': enum_name,
				'name_clean': clean_name,
				'aliases': set(),
				'value': enum.get('value'),
				'type': enum.get('type')
			}
		if enum_alias:
			self.__enums_db[enum_name]['aliases'].add(enum_alias)

	def __load_feature(self, feature):
		# Load a feature element, updating the functions and enums databases
		if not self.__is_wanted_api(feature.get('name')):
			return
		api = feature.get('api')
		version = feature.get('number')
		for require in feature.findall('require'):
			if require.get('profile'):
				continue
			for command in require.findall('command'):
				# set minimal version of GL/GL ES for use function
				func_name = command.get('name')
				if func_name in self.__functions_db:
					if api == 'gl':
						self.__functions_db[func_name]['min_gl_version'] = self.__parse_version(version)
					elif api == 'gles2':
						self.__functions_db[func_name]['min_gles_version'] = self.__parse_version(version)
					# add to function order list
					func_name_clean = self.__functions_db[func_name]['name_clean']
					if func_name == func_name_clean and not func_name in self.__functions_order:
						self.__functions_order.append(func_name)
			for enum in require.findall('enum'):
				enum_name = enum.get('name')
				if enum_name in self.__enums_db:
					enum_name_clean = self.__enums_db[enum_name]['name_clean']
					if enum_name == enum_name_clean and not enum_name in self.__enums_order:
						self.__enums_order.append(enum_name)
	
	def __load_extension(self, extension):
		# parse extension
		ext_name = extension.get('name')
		if not self.__is_wanted_extension(ext_name):
			return
		for require in extension.findall('require'):
			# definition with profile not found and not used in Luanti Irrlicht
			if require.get('profile'):
				continue
			for command in require.findall('command'):
				# update function info about extensions
				func_name = command.get('name')
				self.__functions_db[func_name]['defined_extensions'].add(ext_name)
				if not func_name in self.__functions_order:
						self.__functions_order.append(func_name)

			for enum in require.findall('enum'):
				# add enum to enum order list
				enum_name = enum.get('name')
				if enum_name in self.__enums_db:
					enum_name_clean = self.__enums_db[enum_name]['name_clean']
					if enum_name == enum_name_clean and not enum_name in self.__enums_order:
						self.__enums_order.append(enum_name)
	
	def __load_remove(self, feature):
		# Load a remove XML element, updating the functions and enums databases
		for command in feature.findall('remove/command'):
			func_name = command.get('name')
			if func_name in self.__functions_order:
				self.__functions_order.remove(func_name)
		for enum in feature.findall('remove/enum'):
			enum_name = enum.get('name')
			if enum_name in self.__enums_order:
				self.__enums_order.remove(enum_name)

	def parse_xml(self, xml_file: str):
		# Parse the XML file and load typedefs, commands, enums, features, and extensions
		tree = ET.parse(xml_file)
		root = tree.getroot()

		for typedef in root.findall('types/type'):
			self.__load_typedef(typedef)


		for command in root.findall('commands/command'):
			self.__load_command(command)

		for command in root.findall('commands/command'):
			self.__load_alias(command)

		for enum in root.findall('enums/enum'):
			self.__load_enum(enum)

		for feature in root.findall('feature'):
			self.__load_feature(feature)

		for extension in root.findall('extensions/extension'):
			self.__load_extension(extension)

		for feature in root.findall('feature'):
			self.__load_remove(feature)

	def generate_typedefs(self):
		text = ''
		for typedef in self.__typedefs_db:
			text = '{}\t{}\n'.format(text, typedef)
		return text
	
	def generate_function_typedefs(self):
		text = ''
		for func_name in self.__functions_order:
			function = self.__functions_db[func_name]
			if not function['have_alias']:
				text = '{}\ttypedef {} (APIENTRYP PFN{}PROC_MT) ({});\n'.format(text, function["return"], function["name_clean"].upper(), function['parameters'])
		return text

	def generate_function_pointers(self):
		text = ''
		for func_name in self.__functions_order:
			function = self.__functions_db[func_name]
			if not function['have_alias']:
				text = '{}\tPFN{}PROC_MT {} = NULL;\n'.format(text, function["name_clean"].upper(), function["name_clean"][2:])
				if function['name_clean']=='glGetnUniformdv':
					print(function)
		return text

	def generate_constants(self):
		text = ''
		for enum_name in self.__enums_order:
			enum = self.__enums_db[enum_name]
			if enum['name'] == enum_name:
				enum_type = 'GLenum'
				if enum['type']=="ull":
					enum_type = 'GLuint64'
				text = '{}\tstatic constexpr const {} {} = {}{};\n'.format(text, enum_type, enum['name_clean'][3:], enum['value'], enum['type'] or '')
		return text
	
	def generate_loaders(self):
		text = ''
		for func_name in self.__functions_order:
			function = self.__functions_db[func_name]
			if not function['have_alias']:
				text = '{}\tif (!{}) {} = (PFN{}PROC_MT)cmgr->getProcAddress("{}");\n'.format(text, function["name_clean"][2:], function["name_clean"][2:], function["name_clean"].upper(), func_name)
				for alias_name in function['aliases']:
					if alias_name in self.__functions_order:
						alias = self.__functions_db[alias_name]
						text = '{}\tif (!{}) {} = (PFN{}PROC_MT)cmgr->getProcAddress("{}");\n'.format(text, function["name_clean"][2:], function["name_clean"][2:], function["name_clean"].upper(), alias_name)
		return text

	def get_function_info(self, function_name: str) -> Optional[Dict[str, Optional[Tuple[int, int]]]]:
		return self.__functions_db.get(function_name)
	  
def generate_header(template_file, output_file, gl_info, source_xml):
  # Read the template file
  with open(template_file, 'r') as template_file:
	  template_content = template_file.read()

  # Create a Template object
  template = Template(template_content)

  # Define the values to be substituted in the template
  values = {
	  'source_xml': source_xml,
	  'typedefs': gl_info.generate_typedefs(),
	  'function_typedefs': gl_info.generate_function_typedefs(),
	  'function_pointers': gl_info.generate_function_pointers(),
	  'constants': gl_info.generate_constants()
  }

  # Substitute the values in the template
  output_content = template.substitute(values)

  # Write the output to a new file
  with open(output_file, 'w') as output_file:
	  output_file.write(output_content)

def generate_loader(template_file, output_file, gl_info):
  # Read the template file
  with open(template_file, 'r') as template_file:
	  template_content = template_file.read()

  # Create a Template object
  template = Template(template_content)

  # Define the values to be substituted in the template
  values = {
	  'pointers_load': gl_info.generate_loaders()
  }

  # Substitute the values in the template
  output_content = template.substitute(values)

  # Write the output to a new file
  with open(output_file, 'w') as output_file:
	  output_file.write(output_content)

# header and loader generation
if __name__ == "__main__":
	if len(sys.argv) != 6:
		print("Usage:")
		print("  python3 BindingGenerator.py gl.xml header.template header.h loader.template loader.cpp")
		exit()

	xml_file = sys.argv[1]
	header_temp = sys.argv[2]
	header_h = sys.argv[3]
	loader_temp = sys.argv[4]
	loader_cpp = sys.argv[5]

	gl_info = GLInfo()
	gl_info.parse_xml(xml_file)
	
	generate_header(header_temp, header_h, gl_info, xml_file)
	generate_loader(loader_temp, loader_cpp, gl_info)

	# Show parsed info about function, for debug purposes
	#function_names = ["glReadnPixelsARB", "glReadnPixels"]
	function_names = []
	for function_name in function_names:
		info = gl_info.get_function_info(function_name)
		if info:
			print(f"Function: {function_name}")
			print(f"Minimum OpenGL Version: {info['min_gl_version']}")
			print(f"Minimum OpenGL ES Version: {info['min_gles_version']}")
			print(f"Defined in Extensions: {', '.join(info['defined_extensions'])}")
			print(f"aliases: {info['aliases']}")
			print(f"have alias: {info['have_alias']}")
			print(f"return: {info['return']}")
			print(f"parameters: {info['parameters']}")
		else:
			print(f"Function {function_name} not found in the database.")
