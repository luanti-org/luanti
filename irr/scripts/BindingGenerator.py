#!/usr/bin/env python3
import xml.etree.ElementTree as ET
import sys
from typing import List, Dict, Set, Optional, Tuple
from string import Template
class GLInfo:
	def __init__(self):
		self.__typedefs_db: List[str] = []
		self.__functions_db: Dict[str, dict] = {}
		self.__functions_order: List[str] = []
		self.__enums_db: Dict[str, dict] = {}
		self.__enums_order: List[str] = []
		self.__excluded_extensions = ["GL_ATI_fragment_shader", "GL_AMD_compressed_3DC_texture", "GL_EXT_422_pixels", "GL_EXT_multisample", "GL_SGIS_multisample"]
		self.__ignore_api = ["GL_VERSION_ES_CM_1_0"]
		self.__unwanted_suffixes = ["_ANGLE", "ANGLE", "_APPLE", "APPLE", "_ARB", "ARB", "_EXT", "EXT", "_KHR", "KHR", "_NV", "NV"]
		# functions loaded manually in template
		self.__noload_functions = ["glGetStringi"]
	def __parse_version(self, version: str) -> Tuple[int, int]:
		major, minor = map(int, version.split('.'))
		return major, minor
	def __is_basic_function(self, function: dict) -> bool:
		return function['min_gl_version'] == (1, 0) and function['min_gles_version'] == (2, 0)
	def __is_wanted_extension(self, extension_name: str) -> bool:
		return not any(keyword in extension_name for keyword in self.__excluded_extensions)
	def __is_wanted_api(self, feature_name: str) -> bool:
		return not any(keyword in feature_name for keyword in self.__ignore_api)
	def __remove_suffix(self, text: str) -> str:
		for suffix in self.__unwanted_suffixes:
			if text.endswith(suffix):
				return text[:-len(suffix)]
		return text
	def __load_typedef(self, typedef):
		if not typedef.get('name'):
			type_text = typedef.text or ''
			for defpart in typedef:
				if defpart.tag == 'apientry':
					type_text += "APIENTRY"
				if defpart.text:
					type_text += defpart.text
				if defpart.tail:
					type_text += defpart.tail
			self.__typedefs_db.append(type_text)
	def __load_command(self, command):
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
			if parameters == "":
				parameters = "void"
			ret_type = [text for text in proto.itertext()]
			ret_type = ''.join(ret_type[:-1])
			ret_type = ret_type.strip()
			clean_name = self.__remove_suffix(func_name)
			if func_alias is None:
				clean_name = func_name
			self.__functions_db[func_name] = {
				'name': func_name,
				'name_clean': clean_name,
				'min_gl_version': None,
				'min_gles_version': None,
				'defined_extensions': set(),
				'defined_gl_extensions': set(),
				'defined_gles_extensions': set(),
				'aliases': set(),
				'have_alias': (func_alias is not None),
				'return': ret_type,
				'parameters': parameters
			}
	def __load_alias(self, command):
		proto = command.find('proto')
		func_name = proto.find('name').text
		func_alias = None
		if command.find('alias') is not None:
			func_alias = command.find('alias').get('name')
		if func_alias and func_alias in self.__functions_db:
			self.__functions_db[func_alias]['aliases'].add(func_name)
	def __load_enum(self, enum):
		enum_name = enum.get('name')
		enum_alias = None
		if enum.get('alias') is not None:
			enum_alias = enum_name
			enum_name = enum.get('alias')
		clean_name = self.__remove_suffix(enum_name)
		if enum_alias is None:
			if clean_name in self.__enums_db and self.__enums_db[clean_name]['value'] == enum.get('value'):
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
		if not self.__is_wanted_api(feature.get('name')):
			return
		api = feature.get('api')
		version = feature.get('number')
		for require in feature.findall('require'):
			if require.get('profile'):
				continue
			for command in require.findall('command'):
				func_name = command.get('name')
				if func_name in self.__functions_db:
					if api == 'gl':
						self.__functions_db[func_name]['min_gl_version'] = self.__parse_version(version)
					elif api == 'gles2':
						self.__functions_db[func_name]['min_gles_version'] = self.__parse_version(version)
					func_name_clean = self.__functions_db[func_name]['name_clean']
					if func_name == func_name_clean and func_name not in self.__functions_order:
						self.__functions_order.append(func_name)
			for enum in require.findall('enum'):
				enum_name = enum.get('name')
				if enum_name in self.__enums_db:
					enum_name_clean = self.__enums_db[enum_name]['name_clean']
					if enum_name == enum_name_clean and enum_name not in self.__enums_order:
						self.__enums_order.append(enum_name)
	def __load_extension(self, extension):
		ext_name = extension.get('name')
		if not self.__is_wanted_extension(ext_name):
			return
		api = extension.get('supported').split('|')
		support_gl = ('gl' in api)
		support_gles = ('gles2' in api)
		for require in extension.findall('require'):
			if require.get('profile'):
				continue
			api = require.get('api')
			only_gl = (api == 'gl') or (not api and support_gl and not support_gles)
			only_gles = (api == 'gles2') or (not api and not support_gl and support_gles)
			if api and not (only_gl or only_gles):
				continue
			for command in require.findall('command'):
				func_name = command.get('name')
				if func_name in self.__functions_db:
					if only_gl:
						self.__functions_db[func_name]['defined_gl_extensions'].add(ext_name)
					elif only_gles:
						self.__functions_db[func_name]['defined_gles_extensions'].add(ext_name)
					else:
						self.__functions_db[func_name]['defined_extensions'].add(ext_name)
					if func_name not in self.__functions_order:
						self.__functions_order.append(func_name)
			for enum in require.findall('enum'):
				enum_name = enum.get('name')
				if enum_name in self.__enums_db:
					enum_name_clean = self.__enums_db[enum_name]['name_clean']
					if enum_name == enum_name_clean and enum_name not in self.__enums_order:
						self.__enums_order.append(enum_name)
	def __load_remove(self, feature):
		for command in feature.findall('remove/command'):
			func_name = command.get('name')
			if func_name in self.__functions_order:
				self.__functions_order.remove(func_name)
		for enum in feature.findall('remove/enum'):
			enum_name = enum.get('name')
			if enum_name in self.__enums_order:
				self.__enums_order.remove(enum_name)
	def parse_xml(self, xml_file: str):
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
	def generate_typedefs(self) -> str:
		text = ''
		for typedef in self.__typedefs_db:
			text = '{}\t{}\n'.format(text, typedef)
		return text
	def generate_function_typedefs(self) -> str:
		text = ''
		for func_name in self.__functions_order:
			function = self.__functions_db[func_name]
			if not function['have_alias']:
				text = '{}\ttypedef {} (APIENTRYP PFN{}PROC_MT) ({});\n'.format(text, function["return"], function["name_clean"].upper(), function['parameters'])
		return text
	def generate_function_pointers(self) -> str:
		text = ''
		for func_name in self.__functions_order:
			function = self.__functions_db[func_name]
			if not function['have_alias']:
				text = '{}\tPFN{}PROC_MT {} = NULL;\n'.format(text, function["name_clean"].upper(), function["name_clean"][2:])
		return text
	def generate_constants(self) -> str:
		text = ''
		for enum_name in self.__enums_order:
			enum = self.__enums_db[enum_name]
			if enum['name'] == enum_name:
				enum_type = 'GLenum'
				if enum['type'] == "ull":
					enum_type = 'GLuint64'
				text = '{}\tstatic constexpr const {} {} = {}{};\n'.format(text, enum_type, enum['name_clean'][3:], enum['value'], enum['type'] or '')
		return text
	def generate_basic_loaders(self) -> str:
		text = ''
		for func_name in self.__functions_order:
			if func_name in self.__noload_functions:
				continue
			function = self.__functions_db[func_name]
			if self.__is_basic_function(function):
				if not function['have_alias']:
					text = '{}\tif (!{}) {} = (PFN{}PROC_MT)cmgr->getProcAddress("{}");\n'.format(text, function["name_clean"][2:], function["name_clean"][2:], function["name_clean"].upper(), func_name)
		return text
	def generate_conditional_loaders(self) -> str:
		text = ''
		# Helper function to determine priority by suffix
		def get_alias_priority(name: str) -> int:
			if name.endswith('ARB'):
				return 1  # ARB has highest priority
			if name.endswith('EXT'):
				return 2  # EXT is second
			return 3	  # Vendors (SGIS, NV, ATI...) have lowest priority
		version_vars = {}
		version_statements = {}
		extension_statements = {}
		for func_name in self.__functions_order:
			if func_name in self.__noload_functions:
				continue
			function = self.__functions_db[func_name]
			def process_entity(entity_name: str, entity_data: dict, is_base: bool):
				if is_base and self.__is_basic_function(entity_data):
					return
				ptr_name = function["name_clean"][2:]
				ptr_type = f'PFN{function["name_clean"].upper()}PROC_MT'
				# Setup Version checks
				min_gl = entity_data.get('min_gl_version')
				min_gles = entity_data.get('min_gles_version')
				ver_cond = None
				if min_gl is not None and min_gles is not None:
					ver_cond = f'CHECK_VER({min_gl[0]},{min_gl[1]},{min_gles[0]},{min_gles[1]})'
				elif min_gl is not None:
					ver_cond = f'CHECK_VER_GL({min_gl[0]},{min_gl[1]})'
				elif min_gles is not None:
					ver_cond = f'CHECK_VER_GLES({min_gles[0]},{min_gles[1]})'
				if ver_cond:
					if ver_cond not in version_vars:
						# Extract condition into a valid bool name (e.g. gl3_1_3_0)
						var_name = ver_cond.replace('CHECK_VER_GLES', 'gles').replace('CHECK_VER_GL', 'gl').replace('CHECK_VER', 'gl')
						var_name = var_name.replace('(', '').replace(')', '').replace(',', '_')
						version_vars[ver_cond] = var_name
						version_statements[ver_cond] = []
					var_name = version_vars[ver_cond]
					line = f'\tif (!{ptr_name} && {var_name}) {ptr_name} = ({ptr_type})cmgr->getProcAddress("{entity_name}");\n'
					version_statements[ver_cond].append(line)
				# Setup Extension checks
				exts = []
				for ext in entity_data['defined_extensions']:
					exts.append(f'CHECK_EXT("{ext}")')
				for ext in entity_data['defined_gl_extensions']:
					exts.append(f'CHECK_EXT_GL("{ext}")')
				for ext in entity_data['defined_gles_extensions']:
					exts.append(f'CHECK_EXT_GLES("{ext}")')
				for ext_cond in exts:
					if ext_cond not in extension_statements:
						extension_statements[ext_cond] = []
					line = f'\t\tif (!{ptr_name}) {ptr_name} = ({ptr_type})cmgr->getProcAddress("{entity_name}");\n'
					extension_statements[ext_cond].append(line)
			if not function['have_alias']:
				# Base function
				process_entity(func_name, function, True)
				# Sort aliases by priority (first ARB, then EXT, then rest), alphabetically for stability
				sorted_aliases = sorted(function['aliases'], key=lambda x: (get_alias_priority(x), x))
				for alias_name in sorted_aliases:
					if alias_name in self.__functions_order:
						alias_data = self.__functions_db[alias_name]
						process_entity(alias_name, alias_data, False)
		# 1. Output cached version variables
		if version_vars:
			for ver_cond, var_name in version_vars.items():
				text += f'\tconst bool {var_name} = {ver_cond};\n'
			text += '\n'
		# 2. Output version-dependent loaders
		for ver_cond in version_vars.keys():
			for line in version_statements[ver_cond]:
				text += line
		if version_vars and extension_statements:
			text += '\n'
		# 3. Output extension-dependent loaders in block scopes
		for ext_cond in sorted(extension_statements.keys()):
			text += f'\tif ({ext_cond}) {{\n'
			for line in extension_statements[ext_cond]:
				text += line
			text += '\t}\n'
		return text
	def get_function_info(self, function_name: str) -> Optional[dict]:
		return self.__functions_db.get(function_name)
def generate_header(template_path: str, output_path: str, gl_info: GLInfo, source_xml: str):
	with open(template_path, 'r') as f_in:
		template_content = f_in.read()
	template = Template(template_content)
	values = {
		'source_xml': source_xml,
		'typedefs': gl_info.generate_typedefs(),
		'function_typedefs': gl_info.generate_function_typedefs(),
		'function_pointers': gl_info.generate_function_pointers(),
		'constants': gl_info.generate_constants()
	}
	output_content = template.substitute(values)
	with open(output_path, 'w') as f_out:
		f_out.write(output_content)
def generate_loader(template_path: str, output_path: str, gl_info: GLInfo):
	with open(template_path, 'r') as f_in:
		template_content = f_in.read()
	template = Template(template_content)
	values = {
		'basic_pointers_load': gl_info.generate_basic_loaders(),
		'cond_pointers_load': gl_info.generate_conditional_loaders()
	}
	output_content = template.substitute(values)
	with open(output_path, 'w') as f_out:
		f_out.write(output_content)
if __name__ == "__main__":
	if len(sys.argv) != 6:
		print("Usage:")
		print("  python3 BindingGenerator.py gl.xml header.template header.h loader.template loader.cpp")
		sys.exit(1)
	xml_file = sys.argv[1]
	header_temp = sys.argv[2]
	header_h = sys.argv[3]
	loader_temp = sys.argv[4]
	loader_cpp = sys.argv[5]
	gl_info = GLInfo()
	gl_info.parse_xml(xml_file)
	generate_header(header_temp, header_h, gl_info, xml_file)
	generate_loader(loader_temp, loader_cpp, gl_info)
