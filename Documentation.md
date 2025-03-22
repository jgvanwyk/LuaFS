# LuaFS Documentation

## Usage

LuaFS provides a single module called `filesystem`, which can be imported as follows:
```
local filesystem = require("filesystem")
```
The `filesystem` module is described in the [API](#api) section.

## API

### `filesystem.FileAttributes(path)`
Returns a table of attributes for the file at the given `path`. If an error occurs, then returns `nil` and a `string` describing the error.

The returned table contains the following information:
|Key|Value|
|-|-|
|`type`|A `string` describing the type of the file. The possible values are `"BlockDevice"`, `"CharacterDevice"`, `"Directory"`, `"NamedPipe"`, `"RegularFile"`, `"SymbolicLink"`, `"Socket"` and `"Other"`.|
|`modificationTime`|A `number` representing the Unix time value at which the file’s data was last modified.|
|`changeTime`|A `number` representing the Unix time value at which the file’s status was last changed .|
|`creationTime`|A `number` representing the Unix time value at which the file was created.|
|`size`|A `number` representing the size of the file in bytes.|

### `filesystem.DirectoryIterator(path[, options])`
Returns an iterator into the contents of the directory at the given `path`. If an error occurs, then returns `nil` and a `string` describing the error.

The behaviour of the iterator is controlled by the `options` parameter, which is a table that may contain the following information:
|Key|Value|
|-|-|
|`iterateSubdirectories`|A `bool` that specifies whether the iterator should recursively iterate through all subdirectories of the given directory.|
|`includeFileAttributes`|A `bool` that specifies whether the iterator should return file attributes during iteration.|

The returned iterator will iterate through each file in the directory and return its path as the first return value. If `iterateSubdirectories` is true, then the iterator will iterate through all subdirectories recursively. If `includeFileAttributes` is true, then the iterator will also return a table of attributes for the file as if you had called [`FileAttributes`](#filesystemfileattributespath).
If an error occurs during iteration, then the returned path or attributes table will be `nil`, and a `string` will be returned describing the error.

The returned iterator is a callable object with the following methods:
|Method|Description|
|-|-|
|`skipDescendants()`|Tells the iterator not to iterate through the descendants of the most recently returned file path.|
|`close()`|Tells the iterator to cease iteration and close the directory passed to [`DirectoryIterator`](#filesystemdirectoryiteratorpath-options). The next time the iterator is called, it will return `nil`. Note that the directory is closed automatically when the iterator completes or when it is collected by the garbage collector.|

#### Examples
List all the files in a given directory, including their file sizes.
```lua
for filePath, fileAttributes in filesystem.DirectoryIterator("/path/to/directory", { includeFileAttributes = true }) do
    print(filePath, fileAttributes.size)
end
```

List all the files in a given directory, its subdirectories, their subdirectories, and so on, except for the files in a given list of ignored directories. Also, display any errors that occur.
```lua
local ignoredDirectoryPaths = { "/path/to/ignored/directory/1", "/path/to/ignore/directory/2" }

local directoryIterator = filesystem.DirectoryIterator("/path/to/directory", { iterateSubdirectories = true })

while true do
    local filePath, err = directoryIterator()
    if not err then
        if not filePath then break end
        if ignoredDirectoryPaths[filePath] then
            directoryIterator:skipDescendants()
        end
        print(filePath)
    else
        print(err)
    end
end

```

### `filesystem.CanonicalPath(path)`
Returns the canonical path for the given file `path`. If an error occurs, then returns `nil` and a `string` describing the error.

### `filesystem.DirectoryPath(path)`
Returns the path of the directory containing the file at the given `path`. If an error occurs, then returns `nil` and a `string` describing the error.

### `filesystem.FileName(path)`
Returns the name of the file at the given `path`. If an error occurs, then returns `nil` and a `string` describing the error.

### `filesystem.ChangeDirectory(path)`
Changes the current working directory to the directory at the given `path`. If successful, then returns `true`, and otherwise returns `false` and a `string` describing the error.

### `filesystem.CurrentDirectory()`
Returns the current working directory. If an error occurs, then returns `nil` and a `string` describing the error.