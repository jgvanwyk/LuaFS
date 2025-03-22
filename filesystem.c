#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdlib.h>
#include <fts.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include "lua.h"
#include "lauxlib.h"

static void PushFileAttributes(lua_State *L, struct stat *st);
static int FileAttributes(lua_State *L);
static int DirectoryIterator(lua_State *L);
static int DirectoryIteratorNext(lua_State *L);
static int DirectoryIteratorSkipDescendants(lua_State *L);
static int DirectoryIteratorClose(lua_State *L);
static int CanonicalPath(lua_State *L);
static int DirectoryPath(lua_State *L);
static int FileName(lua_State *L);
static int ChangeDirectory(lua_State *L);
static int CurrentDirectory(lua_State *L);

#define DIRECTORY_ITERATOR_ITERATE_SUBDIRECTORIES 1 << 0
#define DIRECTORY_ITERATOR_INCLUDE_FILE_ATTRIBUTES 1 << 1

#define NANO_SECONDS_PER_SECOND 1e9

static const luaL_Reg filesystem[] = {
    "FileAttributes", FileAttributes,
    "DirectoryIterator", DirectoryIterator,
    "CanonicalPath", CanonicalPath,
    "DirectoryPath", DirectoryPath,
    "FileName", FileName,
    "ChangeDirectory", ChangeDirectory,
    "CurrentDirectory", CurrentDirectory,
    NULL, NULL
};

typedef struct DirectoryIteratorState {
    FTS *ftsp;
    FTSENT *currentEntry;
    unsigned int iterationFlags;
} DirectoryIteratorState;

void PushFileAttributes(lua_State *L, struct stat *st) {
    lua_createtable(L, 0, 5);
    if (S_ISBLK(st->st_mode))
        lua_pushstring(L, "BlockDevice");
    else if (S_ISCHR(st->st_mode))
        lua_pushstring(L, "CharacterDevice");
    else if (S_ISDIR(st->st_mode))
        lua_pushstring(L, "Directory");
    else if (S_ISFIFO(st->st_mode))
        lua_pushstring(L, "NamedPipe");
    else if (S_ISREG(st->st_mode))
        lua_pushstring(L, "RegularFile");
    else if (S_ISLNK(st->st_mode))
        lua_pushstring(L, "SymbolicLink");
    else if (S_ISSOCK(st->st_mode))
        lua_pushstring(L, "Socket");
    else
        lua_pushstring(L, "Other");
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, st->st_mtimespec.tv_sec + st->st_mtimespec.tv_nsec / NANO_SECONDS_PER_SECOND);
    lua_setfield(L, -2, "modificationTime");
    lua_pushinteger(L, st->st_ctimespec.tv_sec + st->st_ctimespec.tv_nsec / NANO_SECONDS_PER_SECOND);
    lua_setfield(L, -2, "changeTime");
    lua_pushinteger(L, st->st_birthtimespec.tv_sec + st->st_birthtimespec.tv_nsec / NANO_SECONDS_PER_SECOND);
    lua_setfield(L, -2, "creationTime");
    lua_pushinteger(L, st->st_size);
    lua_setfield(L, -2, "size");
}

int FileAttributes(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    struct stat st;
    if (stat(path, &st) == 0) {
        PushFileAttributes(L, &st);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not get file information for %s: %s", path, strerror(errno));
        return 2;
    }
}

int CanonicalPath(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    char canonicalPath[MAXPATHLEN];
    if (realpath(path, canonicalPath)) {
        lua_pushstring(L, canonicalPath);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not get canonical path of %s: %s", path, strerror(errno));
        return 2;
    }
}

int FileName(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    char *fileName = basename(path);
    if (fileName) {
        lua_pushstring(L, fileName);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not get file name of %s: %s", path, strerror(errno));
        return 2;
    }
}

int DirectoryPath(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    char *directoryPath = dirname(path);
    if (directoryPath) {
        lua_pushstring(L, directoryPath);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not get directory name of %s: %s", path, strerror(errno));
        return 2;
    }
}

int DirectoryIterator(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    char *paths[] = {(char *)path, NULL};
    DirectoryIteratorState *state = lua_newuserdata(L, sizeof *state);
    state->ftsp = NULL;
    state->currentEntry = NULL;
    state->iterationFlags = 0;
    luaL_getmetatable(L, "filesystem.DirectoryIterator");
    lua_setmetatable(L, -2);
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "iterateSubdirectories");
        if (lua_toboolean(L, -1))
            state->iterationFlags |= DIRECTORY_ITERATOR_ITERATE_SUBDIRECTORIES;
        lua_pop(L, 1);
        lua_getfield(L, 2, "includeFileAttributes");
        if (lua_toboolean(L, -1))
            state->iterationFlags |= DIRECTORY_ITERATOR_INCLUDE_FILE_ATTRIBUTES;
        lua_pop(L, 1);
    }
    int options = FTS_PHYSICAL | FTS_NOCHDIR;
    if (!(state->iterationFlags & DIRECTORY_ITERATOR_INCLUDE_FILE_ATTRIBUTES))
        options |= FTS_NOSTAT;
    state->ftsp = fts_open(paths, options, NULL);
    if (state->ftsp) {
        fts_read(state->ftsp);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not open %s: %s", path, strerror(errno));
        return 2;
    }
}

int DirectoryIteratorNext(lua_State *L) {
    DirectoryIteratorState *state = luaL_checkudata(L, 1, "filesystem.DirectoryIterator");
    if (!state->ftsp)
        return 0;
    FTSENT *entry = fts_read(state->ftsp);
    int resultCount = 1;
start:
    state->currentEntry = entry;
    if (entry) {
        if (entry->fts_info == FTS_D) {
            if (!(state->iterationFlags & DIRECTORY_ITERATOR_ITERATE_SUBDIRECTORIES))
                fts_set(state->ftsp, entry, FTS_SKIP);
        } else if (entry->fts_info == FTS_DP) {
            state->currentEntry = NULL;
            for (;;) {
                entry = fts_read(state->ftsp);
                if (!entry || entry->fts_info != FTS_DP)
                    goto start;
            }
        }
        lua_pushstring(L, entry->fts_path);
        if (entry->fts_info == FTS_NS) {
            if (state->iterationFlags & DIRECTORY_ITERATOR_INCLUDE_FILE_ATTRIBUTES) {
                lua_pushnil(L);
                resultCount++;
            }
            lua_pushfstring(L, "Could not get file information for %s: %s", entry->fts_path, strerror(errno));
            resultCount++;
        } else {
            if (state->iterationFlags & DIRECTORY_ITERATOR_INCLUDE_FILE_ATTRIBUTES) {
                PushFileAttributes(L, entry->fts_statp);
                resultCount++;
            } else if (entry->fts_info == FTS_DNR || entry->fts_info == FTS_ERR) {
                lua_pushfstring(L, "Could not read %s: %s", entry->fts_path, strerror(entry->fts_errno));
                resultCount++;
            }
        }
    } else {
        lua_pushnil(L);
        if (state->iterationFlags & DIRECTORY_ITERATOR_INCLUDE_FILE_ATTRIBUTES) {
            lua_pushnil(L);
            resultCount++;
        }
        if (errno != 0) {
            lua_pushfstring(L, "An error occured: %s", strerror(errno));
            resultCount++;
        }
        fts_close(state->ftsp);
        state->ftsp = NULL;
    }
    return resultCount;
}

int DirectoryIteratorSkipDescendants(lua_State *L) {
    DirectoryIteratorState *state = luaL_checkudata(L, 1, "filesystem.DirectoryIterator");
    if (state->currentEntry)
        fts_set(state->ftsp, state->currentEntry, FTS_SKIP);
    return 0;
}

int DirectoryIteratorClose(lua_State *L) {
    DirectoryIteratorState *state = luaL_checkudata(L, 1, "filesystem.DirectoryIterator");
    if (state->ftsp) {
        fts_close(state->ftsp);
        state->ftsp = NULL;
    }
    return 0;
}

static int CurrentDirectory(lua_State *L) {
    char directoryPath[MAXPATHLEN];
    if (getcwd(directoryPath, MAXPATHLEN)) {
        lua_pushstring(L, directoryPath);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not get current directory: %s", strerror(errno));
        return 2;
    }
}

static int ChangeDirectory(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (chdir(path) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "Could not change directory to %s: %s", path, strerror(errno));
        return 2;
    }
}

int luaopen_filesystem(lua_State *L) {
    luaL_newmetatable(L, "filesystem.DirectoryIterator");
    lua_pushcfunction(L, DirectoryIteratorClose);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, DirectoryIteratorClose);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, DirectoryIteratorNext);
    lua_setfield(L, -2, "__call");
    lua_pushcfunction(L, DirectoryIteratorSkipDescendants);
    lua_setfield(L, -2, "skipDescendants");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushstring(L, "protected metatable");
    lua_setfield(L, -2, "__metatable");
    luaL_register(L, "filesystem", filesystem);
    return 1;
}