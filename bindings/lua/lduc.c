
/*
 * Basic Lua binding to libduc. Allows OO-like access to duc functions. Does
 * not yet export all Duc functionality.
 */

#include <stdio.h>
#include <stdlib.h>

#include <duc.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


/*
 * Wrapper structs for userdata
 */

struct duc_wrap {
	struct duc *duc;
};

struct dir_wrap {
	struct duc *duc;
	struct duc_dir *dir;
};


/*
 * Misc functions
 */

#define CHECK_DUC(e) \
	if(!(e)) { \
		lua_pushnil(L); \
		lua_pushstring(L, duc_strerror(duc)); \
		return 2; \
	}


static const char* duc_type_to_string(duc_file_type type)
{
	switch(type) {
		case DUC_FILE_TYPE_BLK: return "blk";
		case DUC_FILE_TYPE_CHR: return "chr";
		case DUC_FILE_TYPE_DIR: return "dir";
		case DUC_FILE_TYPE_FIFO: return "fifo";
		case DUC_FILE_TYPE_LNK: return "lnk";
		case DUC_FILE_TYPE_REG: return "reg";
		case DUC_FILE_TYPE_SOCK: return "sock";
		case DUC_FILE_TYPE_UNKNOWN: return "unknown";
	}

	return "";
}


static void duc_size_to_table(lua_State *L, struct duc_size *size)
{
	lua_newtable(L);

	lua_pushstring(L, "apparent");
	lua_pushnumber(L, size->apparent);
	lua_settable(L, -3);
	
	lua_pushstring(L, "actual");
	lua_pushnumber(L, size->actual);
	lua_settable(L, -3);

	lua_pushstring(L, "count");
	lua_pushnumber(L, size->count);
	lua_settable(L, -3);
}


static struct duc *check_wduc(lua_State *L, struct duc_wrap *wduc)
{
	if(wduc->duc == NULL) {
		lua_pushstring(L, "Accessing closed duc context");
		lua_error(L);
	}
	return wduc->duc;
}


static struct duc_dir *check_wdir(lua_State *L, struct dir_wrap *wdir)
{
	if(wdir->dir == NULL) {
		lua_pushstring(L, "Accessing closed duc dir");
		lua_error(L);
	}
	return wdir->dir;
}


int l_gc(lua_State *L)
{
	struct duc_wrap *w = luaL_checkudata(L, 1, "duc");
	duc_del(w->duc);
	printf("Gc\n");
	return 0;
}


/*
 * Create new duc context, metatable access duc_* functions() in
 * OO-like way
 */

static int l_new(lua_State *L)
{
        struct duc_wrap *w = lua_newuserdata(L, sizeof(struct duc_wrap));

        w->duc = duc_new();

        luaL_getmetatable(L, "duc");
        lua_setmetatable(L, -2);

	return 1;
}

static struct luaL_Reg lduc_table[] = {
	{ "new",               l_new },
	{ NULL,                NULL },
};


/*
 * duc_*() functions accessed through duc object in lua
 */

static int l_duc_open(lua_State *L)
{
	struct duc_wrap *w = luaL_checkudata(L, 1, "duc");
	const char *fname_db = lua_tostring(L, 2);
	struct duc *duc = w->duc;
	
	int r = duc_open(duc, fname_db, DUC_OPEN_RO);
	CHECK_DUC(r == DUC_OK);

	lua_pushboolean(L, true);
	return 0;
}


static int l_duc_dir_open(lua_State *L)
{
	struct duc_wrap *wduc = luaL_checkudata(L, 1, "duc");
	struct duc *duc = check_wduc(L, wduc);
	const char *path = luaL_checkstring(L, 2);
        
	struct duc_dir *dir = duc_dir_open(duc, path);
	CHECK_DUC(dir != NULL);
	
	struct dir_wrap *wdir = lua_newuserdata(L, sizeof(struct duc_wrap));

        wdir->duc = duc_new();
	wdir->dir = dir;

        luaL_getmetatable(L, "duc_dir");
        lua_setmetatable(L, -2);

	return 1;
}


static int l_duc_close(lua_State *L)
{
	struct duc_wrap *wduc = luaL_checkudata(L, 1, "duc");
	if(wduc->duc) {
		duc_close(wduc->duc);
		wduc->duc = NULL;
	}
	return 0;
}


static struct luaL_Reg lduc_meta[] = {
	{ "open",              l_duc_open },
	{ "dir_open",          l_duc_dir_open },
	{ "close",             l_duc_close },
	{ "__gc",              l_duc_close },
	{ NULL,                NULL },
};



static int l_dir_read(lua_State *L)
{
	struct dir_wrap *wdir = luaL_checkudata(L, 1, "duc_dir");
	struct duc_dir *dir = check_wdir(L, wdir);
        
	struct duc_dirent *ent = duc_dir_read(dir, DUC_SIZE_TYPE_APPARENT, DUC_SORT_SIZE);

	if(ent) {
		lua_newtable(L);
		lua_pushstring(L, "name"); lua_pushstring(L, ent->name); lua_settable(L, -3);
		lua_pushstring(L, "size"); duc_size_to_table(L, &ent->size); lua_settable(L, -3);
		lua_pushstring(L, "type"); lua_pushstring(L, duc_type_to_string(ent->type)); lua_settable(L, -3);
		lua_pushstring(L, "dev"); lua_pushnumber(L, ent->devino.dev); lua_settable(L, -3);
		lua_pushstring(L, "ino"); lua_pushnumber(L, ent->devino.ino); lua_settable(L, -3);
	} else {
		lua_pushnil(L);
	}

	return 1;
}


static int l_dir_get_path(lua_State *L)
{
	struct dir_wrap *wdir = luaL_checkudata(L, 1, "duc_dir");
	struct duc_dir *dir = check_wdir(L, wdir);
	struct duc *duc = wdir->duc;
        
	char *path = duc_dir_get_path(dir);
	CHECK_DUC(path != NULL);
	lua_pushstring(L, path);
	free(path);
	return 1;
}


static int l_dir_get_size(lua_State *L)
{
	struct dir_wrap *wdir = luaL_checkudata(L, 1, "duc_dir");
	struct duc_dir *dir = check_wdir(L, wdir);
       
	struct duc_size size;
	duc_dir_get_size(dir, &size);

	duc_size_to_table(L, &size);
	return 1;
}


static int l_dir_get_count(lua_State *L)
{
	struct dir_wrap *wdir = luaL_checkudata(L, 1, "duc_dir");
	struct duc_dir *dir = check_wdir(L, wdir);
	lua_pushnumber(L, duc_dir_get_count(dir));
	return 1;
}


static int l_dir_seek(lua_State *L)
{
	struct dir_wrap *wdir = luaL_checkudata(L, 1, "duc_dir");
	size_t off = luaL_checknumber(L, 2);
	struct duc_dir *dir = check_wdir(L, wdir);
	int r = duc_dir_seek(dir, off);
	lua_pushboolean(L, r == 0);
	return 1;
}


static int l_dir_rewind(lua_State *L)
{
	struct dir_wrap *wdir = luaL_checkudata(L, 1, "duc_dir");
	struct duc_dir *dir = check_wdir(L, wdir);
	int r = duc_dir_rewind(dir);
	lua_pushboolean(L, r == 0);
	return 1;
}


static int l_dir_close(lua_State *L)
{
	struct dir_wrap *wdir = luaL_checkudata(L, 1, "duc_dir");
	if(wdir->dir) {
		duc_dir_close(wdir->dir);
		wdir->dir = NULL;
	}
	return 0;
}


static struct luaL_Reg ldir_meta[] = {
	{ "read",              l_dir_read },
	{ "get_path",          l_dir_get_path },
	{ "get_size",          l_dir_get_size },
	{ "get_count",         l_dir_get_count },
	{ "seek",              l_dir_seek },
	{ "rewind",            l_dir_rewind },
	{ "close",             l_dir_close },
	{ "__gc",              l_dir_close },
	{ NULL,                NULL },
};


int luaopen_lduc(lua_State *L)
{
	luaL_newmetatable(L, "duc");
	lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, lduc_meta, 0);
	
	luaL_newmetatable(L, "duc_dir");
	lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, ldir_meta, 0);

	luaL_newlib(L, lduc_table);
	return 1;
}

/*
 * End
 */
