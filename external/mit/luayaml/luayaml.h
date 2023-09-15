/*
 * Copyright (C) 2018 - 2022 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* YAML for Lua */

#ifndef __LUA_YAML__
#define __LUA_YAML__

#define LUAYAML_LUA_LOAD_TAG		"!Lua/load"
#define LUAYAML_LUA_CALL_TAG		"!Lua/call"
#define LUAYAML_LUA_LOADFILE_TAG	"!Lua/loadfile"
#define LUAYAML_LUA_CALLFILE_TAG	"!Lua/callfile"

#define LUAYAML_FILE_TAG		"!file"

extern int luaopen_yaml(lua_State *L);

#endif /* __LUA_YAML__ */
