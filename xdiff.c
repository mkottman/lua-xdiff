/*

 Lua-xdiff - a Lua binding to the Xdiff library.

 Copyright (c) 2010 Michal Kottman

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/

#include <xdiff.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/* Writing routines */

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

static int write_string(void *priv, mmbuffer_t *mb, int nbuf) {
	luaL_Buffer *buf = (luaL_Buffer*) priv;
	int i;
	for (i = 0; i < nbuf; i++) {
		long total, size = mb[i].size;
		for (total = 0; total < size; total += LUAL_BUFFERSIZE) {
			int tocopy = MIN(size - total, LUAL_BUFFERSIZE);
			char * p = luaL_prepbuffer(buf);
			if (!p)
				return -1;
			memcpy(p, mb[i].ptr + total, tocopy);
			luaL_addsize(buf, tocopy);
		}
	}
	return 0;
}

static int write_file(void *priv, mmbuffer_t *mb, int nbuf) {
	int i;
	for (i = 0; i < nbuf; i++)
		if (!fwrite(mb[i].ptr, mb[i].size, 1, (FILE *) priv))
			return -1;
	return 0;
}

/* Reading routines */

static mmfile_t from_file(lua_State *L, int idx) {
	mmfile_t mf;
	FILE *f = *((FILE**) luaL_checkudata(L, idx, LUA_FILEHANDLE));

	if (xdl_init_mmfile(&mf, LUAL_BUFFERSIZE, XDL_MMF_ATOMIC) < 0)
		luaL_error(L, "Unable to initialize mmfile_t");

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buf = NULL;
	if (!(buf = (char *) xdl_mmfile_writeallocate(&mf, len))) {
		xdl_free_mmfile(&mf);
		luaL_error(L, "Unable to allocate memory for mmfile_t");
	}

	if (fread(buf, (size_t) len, 1, f) == 0) {
		xdl_free_mmfile(&mf);
		luaL_error(L, "Unable to read from file: %s %d", strerror(errno), len);
	}

	return mf;
}

static mmfile_t from_string(lua_State *L, int idx) {
	mmfile_t mf;

	size_t len;
	const char * str = lua_tolstring(L, idx, &len);

	if (xdl_init_mmfile(&mf, LUAL_BUFFERSIZE, XDL_MMF_ATOMIC) < 0)
		luaL_error(L, "Unable to initialize mmfile_t");

	char *buf = NULL;
	if (!(buf = (char *) xdl_mmfile_writeallocate(&mf, len))) {
		xdl_free_mmfile(&mf);
		luaL_error(L, "Unable to allocate memory for mmfile_t");
	}

	memcpy(buf, str, len);

	return mf;
}

static mmfile_t parse_argument(lua_State *L, int idx) {
	int type = lua_type(L, idx);
	if (type == LUA_TSTRING)
		return from_string(L, idx);
	else if (type == LUA_TUSERDATA)
		return from_file(L, idx);
	else
		luaL_argerror(L, idx, "expecting file or string");
}

/* Memory management */

static void *m_alloc(void *priv, unsigned int size) {
	return malloc(size);
}

static void m_free(void *priv, void *ptr) {
	free(ptr);
}

static void *m_realloc(void *priv, void *ptr, unsigned int size) {
	return realloc(ptr, size);
}

static void init_allocator() {
	memallocator_t malt;
	malt.priv = NULL;
	malt.malloc = m_alloc;
	malt.free = m_free;
	malt.realloc = m_realloc;
	xdl_set_allocator(&malt);
}

/* Lua library functions */

static int lxd_diff(lua_State *L) {
	mmfile_t m1 = parse_argument(L, 1);
	mmfile_t m2 = parse_argument(L, 2);

	xpparam_t params = { 0 };
	xdemitconf_t cfg = { 3 };
	xdemitcb_t ecb = { 0 };

	if (lua_gettop(L) == 3) {
		const char *fname = luaL_checkstring(L, 3);
		FILE *res = fopen(fname, "wb");
		ecb.priv = res;
		ecb.outf = write_file;
	} else {
		luaL_Buffer b;
		luaL_buffinit(L, &b);
		ecb.priv = &b;
		ecb.outf = write_string;
	}

	xdl_diff(&m1, &m2, &params, &cfg, &ecb);

	if (lua_gettop(L) == 3) {
		fclose((FILE*) ecb.priv);
		return 0;
	} else {
		luaL_pushresult((luaL_Buffer*) ecb.priv);
		return 1;
	}
}

static luaL_Reg lxd_functions[] = {
	{ "diff", lxd_diff },
	{ NULL, NULL }
};

LUALIB_API int luaopen_xdiff(lua_State *L) {
	init_allocator();
	luaL_register(L, "xdiff", lxd_functions);
	return 1;
}
