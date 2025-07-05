#!/usr/bin/lua
-- BindingGenerator.lua (c) hecks 2021
-- This script is a part of IrrlichtMT, released under the same license.

-- By default we assume you're running this from /scripts/
-- and that you have the necessary headers there (gitignored for your convenience)
local sourceTreePath = os.getenv( "IRRMTREPO" ) or "..";
-- Otherwise run this from wherever you want and set the above env variable.
local glHeaderPath = os.getenv( "GLHEADERPATH" ) or ".";
-- GL headers will be looked for in the current directory or GLHEADERPATH.
-- At the moment we require:
-- 		"glcorearb.h"
-- 		"gl2ext.h"
-- Files other than glcorearb.h are only parsed for vendor specific defines
-- and aliases. Otherwise we only use what exists in glcorearb.h, further
-- restricted to procedures that are either core or ARB.


-- Emulate a portion of the libraries that this was written against.
getmetatable( "" ).__index = string;
getmetatable( "" ).__len = string.len;
getmetatable( "" ).__call = string.format;
function string:Split( pat )
	local r = {};
	local pos = 1;
	local from, to;
	while pos and pos <= #self do
		from, to = self:find( pat, pos );
		if not from then
			break;
		end
		r[#r+1] = self:sub( pos, from - 1 );
		pos = to + 1;
	end
	r[#r+1] = self:sub( pos, #self );
	return r;
end
function string:TrimBothEnds()
	return self:gsub("^%s+",""):gsub("%s+$","");
end
local List;
List = function( t )
	return setmetatable( t or {}, {
		__index = {
			Add = function( t, v )
				t[#t+1] = v;
			end;
			AddFormat = function( t, str, ... )
				t:Add( str( ... ) );
			end;
			Where = function( t, f )
				local r = {};
				for i=1, #t do
					if f(t[i]) then r[#r+1] = t[i]; end
				end
				return List( r );
			end;
			Select = function( t, f )
				local r = {};
				for i=1, #t do
					r[#r+1] = f( t[i] );
				end
				return List( r );
			end;
			Join = function( t, n )
				local r = {};
				for i=1, #t do
					r[i] = t[i];
				end
				for i=1, #n do
					r[#r+1] = n[i];
				end
				return List( r );
			end;
			Concat = table.concat;
		};
	} );
end


------------ Header parsing ------------

-- GL and GLES alike
local driverVendors = {
	"NV", "AMD", "INTEL", "OVR", "QCOM", "IMG", "ANGLE", "APPLE", "MESA"
}
local vendorSuffixes = {
	"ARB", "EXT", "KHR", "OES",
	unpack( driverVendors )
};
local vendorSuffixPattern = {};
local constSuffixPattern = {};
for i=1, #vendorSuffixes do
	vendorSuffixPattern[i] = vendorSuffixes[i] .. "$";
	constSuffixPattern[i] = ("_%s$")( vendorSuffixes[i] );
end
local constBanned = {};
for i=1, #driverVendors do
	constBanned[driverVendors[i]] = true;
end
-- Strip the uppercase extension vendor suffix from a name.
local function StripVendorSuffix( str, const )
	local patterns = const and constSuffixPattern or vendorSuffixPattern;
	local n;
	for i=1, #patterns do
		str, n = str:gsub( patterns[i], "" );
		if n > 0 then
			return str, vendorSuffixes[i];
		end
	end
	return str;
end

-- Normalize the type of an argument or return, also stripping any suffix
-- and normalizing all whitespace regions to single spaces.
local function NormalizeType( str )
	local chunks = str:Split( "%s+" );
	for j=1, #chunks do
		chunks[j] = StripVendorSuffix( chunks[j] );
	end
	local T = table.concat(chunks, " " );
	return T:TrimBothEnds();
end

-- Normalize an argument, returning the normalized type and the name separately,
-- always sticking the * of a pointer to the type rather than the name.
-- We need this to generate a normalized arg list and function signature (below)
local function NormalizeArgument( str )
	local chunks = str:Split( "%s+" );
	for j=1, #chunks do
		chunks[j] = StripVendorSuffix( chunks[j] );
	end
	local last = chunks[#chunks];
	local name = last:match( "[%w_]+$" );
	chunks[#chunks] = #name ~= #last and last:sub( 1, #last-#name) or nil;
	local T = table.concat(chunks, " " ):TrimBothEnds();
	return T, name
end

-- Normalize an argument list so that two matching prototypes
-- will produce the same table if fed to this function.
local function NormalizeArgList( str )
	local args = str:Split( ",%s*" );
	local r = {};
	for i=1, #args do
		local T, name = NormalizeArgument( args[i] );
		r[i] = { T, name };
	end
	return r;
end

-- Normalize a function signature into a unique string for keying
-- in such a way that if two different GL procedures may be assigned
-- to the same function pointer, this will produce an identical string for both.
-- This makes it possible to detect function aliases that may work as a fallback.
-- You still have to check for the appropriate extension.
local function NormalizeFunctionSignature( T, str )
	local args = str:Split( ",%s*" );
	local r = {};
	for i=1, #args do
		r[i] = NormalizeArgument( args[i] );
	end
	return ("%s(%s)")( T, table.concat( r, ", " ) );
end

-- Mangle the PFN name so that we don't collide with a
-- typedef from any of the GL headers.
local pfnFormat = "PFNGL%sPROC_MT";

--( T, name, args )
local typedefFormat = "\ttypedef %s (APIENTRYP %s) (%s);"
-- Generate a PFN...GL style typedef for a procedure
--
local function GetProcedureTypedef( proc )
	local args = {};
	for i=1, #proc.args do
		args[i] = ("%s %s")( unpack( proc.args[i] ) )
	end
	return typedefFormat( proc.retType, pfnFormat( proc.name:upper() ), table.concat( args, ", " ) );
end

local procedures = List();
local nameset = {};
local definitions = List();
local consts = List();

--[[
	Structured procedure representation:

	ProcSpec = {
		string name;		-- Normalized name as it appears in the GL spec
		string? vendor;		-- Uppercase vendor string (ARB, EXT, AMD, NV etc)
		string signature;
		string retType;
		args = { { type, name } };
	};
]]
-- Parse a whole header, extracting the data.
local function ParseHeader( path, into, apiRegex, defs, consts, nameSet, noNewNames )
	defs:AddFormat( "\t// %s", path );
	local f = assert( io.open( path, "r" ) );
	for line in f:lines() do
		-- Do not parse PFN typedefs; they're easily reconstructible.
		local T, rawName, args = line:match( apiRegex );
		if T then
			T = NormalizeType( T );
			-- Strip the 'gl' namespace prefix.
			local procName = rawName:sub(3,-1);
			local name, vendor = StripVendorSuffix( procName );
			if not (noNewNames and nameSet[name]) then
				nameSet[name] = true;
				into:Add{
					name = name;
					vendor = vendor;
					-- pfnType = pfnFormat( procName:upper() );
					signature = NormalizeFunctionSignature( T, args );
					retType = T;
					args = NormalizeArgList( args );
				};
			end
		elseif ( line:find( "#" ) and not line:find( "#include" ) ) then
			local rawName, value = line:match( "#define%s+GL_([_%w]+)%s+(0x%w+)" );
			if rawName and value then
				local name, vendor = StripVendorSuffix( rawName, true );
				if not constBanned[vendor] then
					consts:Add{ name = name, vendor = vendor, value = value };
				end
			end
			::skip::
		elseif( line:find( "typedef" ) and not line:find( "%(" ) ) then
			-- Passthrough non-PFN typedefs
			defs:Add( "\t" .. line );
		end
	end
	defs:Add "";
	f:close();
end

------------ Parse the headers ------------

-- ES/gl2.h is a subset of glcorearb.h and does not need parsing.

local funcRegex = "GLAPI%s+(.+)APIENTRY%s+(%w+)%s*%((.*)%)";
local funcRegexES = "GL_APICALL%s+(.+)GL_APIENTRY%s+(%w+)%s*%((.*)%)";
ParseHeader( glHeaderPath .. "/glcorearb.h", procedures, funcRegex, definitions, consts, nameset );
ParseHeader( glHeaderPath .. "/gl2ext.h", procedures, funcRegexES, List(), consts, nameset, true );
-- Typedefs are redirected to a dummy list here on purpose.
-- The only unique typedef from gl2ext is this:
definitions:Add "\ttypedef void *GLeglClientBufferEXT;";

------------ Sort out constants ------------

local cppConsts = List();
do
	local constBuckets = {};
	for i=1, #consts do
		local vendor = consts[i].vendor or "core";
		constBuckets[consts[i].name] = constBuckets[consts[i].name] or {};
		constBuckets[consts[i].name][vendor] = consts[i].value;
	end
	local names = {};
	for i=1, #consts do
		local k = consts[i].name;
		local b = constBuckets[k];
		if k == "WAIT_FAILED" or k == "DIFFERENCE" then
			-- This is why using #define as const is evil.
			k = "_" .. k;
		end
		if b and not names[k] then
			names[k] = true;
			-- I have empirically tested that constants in GL with the same name do not differ,
			-- at least for these suffixes.
			local v = b.core or b.KHR or b.ARB or b.OES or b.EXT;
			if v then
				local T = v:find( "ull" ) and "GLuint64" or "GLenum";
				cppConsts:AddFormat( "\tstatic constexpr const %s %s = %s;", T, k, v );
			end
		end
	end
end


------------ Sort out procedures ------------

local procTable = {};

local coreProcedures = procedures:Where( function(x) return not x.vendor; end );
local arbProcedures = procedures:Where( function(x) return x.vendor == "ARB"; end );

-- Only consider core and ARB functions.
local nameList = coreProcedures:Join( arbProcedures ):Select(
	function(p)
		return p.name;
	end );

local nameSet = {};
local uniqueNames = List();

for s, k in ipairs( nameList ) do
	if not nameSet[k] then
		nameSet[k] = true;
		uniqueNames:Add( k );
	end
end

for i=1, #procedures do
	local p = procedures[i];
	procTable[p.name] = procTable[p.name] or {};
	local key = p.vendor or "core";
	procTable[p.name][key] = p;
end

local priorityList = List{ "core", "ARB", "OES", "KHR" };

local typedefs = List();
local pointers = List();
local loader = List();

for s, str in ipairs( uniqueNames ) do
	pointers:Add( ("\t%s %s = NULL;")( pfnFormat( str:upper() ), str ) );
	local typeDefGenerated = false;
	for i=1, #priorityList do
		local k = priorityList[i];
		local proc = procTable[str][k]
		if proc then
			if not typeDefGenerated then
				typedefs:Add( GetProcedureTypedef( proc ) );
				typeDefGenerated = true;
			end
			local vendor = k == "core" and "" or k;
			loader:AddFormat(
				'\tif (!%s) %s = (%s)cmgr->getProcAddress("%s");\n',
				str, str, pfnFormat( proc.name:upper() ), ("gl%s%s")(str,vendor)
			);
		end
	end
end


------------ Write files ------------

-- Write loader header
local f = assert(io.open( sourceTreePath .. "/include/mt_opengl.h", "wb" ));
f:write[[
// This code was generated by scripts/BindingGenerator.lua
// Do not modify it, modify and run the generator instead.

#pragma once

#include <string>
#include <unordered_set>
#include "IContextManager.h"
#include <KHR/khrplatform.h>

#ifndef APIENTRY
	#define APIENTRY KHRONOS_APIENTRY
#endif
#ifndef APIENTRYP
	#define APIENTRYP APIENTRY *
#endif
// undefine a few names that can easily clash with system headers
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#ifdef ZERO
#undef ZERO
#endif
#ifdef ONE
#undef ONE
#endif

]];

f:write[[
class OpenGLProcedures final {
private:
]];
f:write( definitions:Concat( "\n" ) );
f:write( "\n" );
-- The script will miss this particular typedef thinking it's a PFN,
-- so we have to paste it in manually. It's the only such type in OpenGL.
f:write[[
	typedef void (APIENTRY *GLDEBUGPROC)
		(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);

]]
f:write( typedefs:Concat( "\n" ) );
f:write( "\n\n" );
f:write [[
	std::unordered_set<std::string> extensions;

public:
	// Call this once after creating the context.
	void LoadAllProcedures(video::IContextManager *cmgr);
	/// Check if an extension is supported.
	/// @param ext full extension name e.g. "GL_KHR_no_error"
	inline bool IsExtensionPresent(const std::string &ext) const
	{
		return extensions.count(ext) > 0;
	}

]];
f:write( pointers:Concat( "\n" ) );
f:write( "\n\n" );
f:write( cppConsts:Concat( "\n" ) );
f:write( "\n\n" );
-- We filter constants not in hex format to avoid the VERSION_X_X and extension
-- defines, but that means we miss these.
f:write[[
	static constexpr const GLenum NO_ERROR = 0;
	static constexpr const GLenum ZERO = 0;
	static constexpr const GLenum ONE = 1;
	static constexpr const GLenum NONE = 0;
]];
f:write( "};\n" );
f:write( "\n// Global GL procedures object.\n" );
f:write( "extern OpenGLProcedures GL;\n" );
f:close();

-- Write loader implementation
f = assert(io.open( sourceTreePath .. "/src/mt_opengl_loader.cpp", "wb" ));
f:write[[
// This code was generated by scripts/BindingGenerator.lua
// Do not modify it, modify and run the generator instead.

#include "mt_opengl.h"
#include <string>
#include <sstream>

OpenGLProcedures GL;

void OpenGLProcedures::LoadAllProcedures(video::IContextManager *cmgr)
{

]];
f:write( loader:Concat() );
f:write[[

	/* OpenGL 3 & ES 3 way to enumerate extensions */
	GLint ext_count = 0;
	GetIntegerv(NUM_EXTENSIONS, &ext_count);
	// clear error which is raised if unsupported
	while (GetError() != GL.NO_ERROR) {}
	extensions.reserve(ext_count);
	for (GLint k = 0; k < ext_count; k++) {
		auto tmp = GetStringi(EXTENSIONS, k);
		if (tmp)
			extensions.emplace((char*)tmp);
	}
	if (!extensions.empty())
		return;

	/* OpenGL 2 / ES 2 way to enumerate extensions */
	auto ext_str = GetString(EXTENSIONS);
	if (!ext_str)
		return;
	// get the extension string, chop it up
	std::istringstream ext_ss((char*)ext_str);
	std::string tmp;
	while (std::getline(ext_ss, tmp, ' '))
		extensions.emplace(tmp);
}
]];
f:close();
