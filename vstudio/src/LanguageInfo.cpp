// Copyright (C) 2022 - 2023 Zukaritasu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "LanguageInfo.h"
#include "PluginInterface.h"


extern NppData nppData;

LanguageInfo GetLanguageInfo(const char* extension)
{
	// returns the default information because the file does not have an
	// extension that identifies it
	if (extension == nullptr || extension[0] == '\0')
		return { "TEXT",  NPP_DEFAULTIMAGE };
	
	LangType current_lang = L_TEXT;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTLANGTYPE, 0, (LPARAM)&current_lang);

	switch (current_lang)
	{
	case L_JAVA:          return { "JAVA", "java" };
	case L_JAVASCRIPT:
	case L_JS:
		                  return { "JAVASCRIPT", "javascript" };
	case L_C:             return { "C", "c" };
	case L_CPP:           return { "C++", "cpp" };
	case L_CS:            return { "C#", "csharp"};
	case L_CSS:           return { "CSS", "css" };
	case L_HASKELL:       return { "HASKELL", "haskell" };
	case L_HTML:          return { "HTML", "html" };
	case L_PHP:           return { "PHP", "php" };
	case L_PYTHON:        return { "PYTHON", "python" };
	case L_RUBY:          return { "RUBY", "ruby" };
	case L_XML:           return { "XML", "xml" };
	case L_VB:            return { "VISUALBASIC", "visualbasic" };
	case L_BASH:
	case L_BATCH:         return { "BATCH", "cmd" };
	case L_LUA:           return { "LUA", "lua" };
	case L_CMAKE:         return { "CMAKE", "cmake" };
	case L_PERL:          return { "PERL", "perl" };
	case L_JSON:          return { "JSON", "json" };
	case L_YAML:          return { "YAML", "yaml" };
	case L_OBJC:          return { "OBJECTIVE-C", "objectivec" };
	case L_RUST:          return { "RUST", "rust" };
	case L_LISP:          return { "LISP", "lisp" };
	case L_R:             return { "R", "r" };
	case L_SWIFT:         return { "SWIFT", "swift" };
	case L_FORTRAN:       return { "FORTRAN", "fortran" };
	case L_ERLANG:        return { "ERLANG", "erlang" };
	case L_COFFEESCRIPT:  return { "COFFEESCRIPT", "coffeescript" };
	case L_RC:            return { "RESOURCE", NPP_DEFAULTIMAGE };
	case L_ASM:             return { "ASSEMBLY", "assembly" };
	case L_SQL:             return { "SQL", "sql" };
	case L_MATLAB:             return { "MATLAB", "matlab" };
	default:
		if (strcmp(extension, ".gitignore") == 0)
			return               { "GIT", "git" };
		// In dark mode it is L_USER but it is ignored 
		if (strcmp(extension, ".md") == 0 || strcmp(extension, ".markdown") == 0)
			return               { "MARKDOWN", "markdown" };
		if (strcmp(extension, ".ts") == 0 || strcmp(extension, ".tsx") == 0)
			return               { "TYPESCRIPT", "typescript" };
		break;
	}
	return { "TEXT", NPP_DEFAULTIMAGE };
}

LanguageInfo& LanguageInfo::operator=(const LanguageInfo& info)
{
	_name = info._name;
	_large_image = info._large_image;
	return *this;
}
