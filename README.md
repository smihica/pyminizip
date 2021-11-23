## pyminizip

To create a password encrypted zip file in python.
And the zip file is able to extract in WINDOWS.

This is a simple Minizip wrapper of python.
(http://www.winimage.com/zLibDll/minizip.html)

This software uses zlib.
License: zlib/libpng License.

## Install
```
$ pip install pyminizip
```

## Install zlib
```
# linux
$ sudo apt-get install zlib
# mac
$ xcode-select --install
$ brew install zlib
```

## How to use

This package just provides three functions.

```
pyminizip.compress("/srcfile/path.txt", "file_path_prefix", "/distfile/path.zip", "password", int(compress_level))
```

  Args:
  1. src file path (string)
  2. src file prefix path (string) or None (path to prepend to file)
  3. dst file path (string)
  4. password (string) or None (to create no-password zip)
  5. compress_level(int) between 1 to 9, 1 (more fast) <---> 9 (more compress) or 0 (default)

  Return value:
  - always returns None

```
pyminizip.compress_multiple([u'pyminizip.so', 'file2.txt'], [u'/path_for_file1', u'/path_for_file2'], "file.zip", "1233", 4, progress)
```

  Args:
  1. src file LIST path (list)
  2. src file LIST prefix path (list) or []
  3. dst file path (string)
  4. password (string) or None (to create no-password zip)
  5. compress_level(int) between 1 to 9, 1 (more fast)  <---> 9 (more compress)
  6. optional function to be called during processing which takes one argument, the count of how many files have been compressed

  Return value:
  - always returns None

```
pyminizip.uncompress("/srcfile/path.zip", "password", "/dirtoextract", int(withoutpath))
```

  Args:
  1. src file path (string)
  2. password (string) or None (to unzip encrypted archives)
  3. dir path to extract files or None (to extract in a specific dir or cwd)
  4. withoutpath (exclude path of extracted)

  Return value:
  - always returns None
