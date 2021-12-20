/*
  Original is minizip.c

  minizip.c
  Version 1.1, February 14h, 2010
  sample part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

  Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

  Modifications of Unzip for Zip64
  Copyright (C) 2007-2008 Even Rouault

  Modifications for Zip64 support on both zip and unzip
  Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )
*/

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "bytesobject.h"

#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
    #ifndef __USE_FILE_OFFSET64
        #define __USE_FILE_OFFSET64
    #endif
    #ifndef __USE_LARGEFILE64
        #define __USE_LARGEFILE64
    #endif
    #ifndef _LARGEFILE64_SOURCE
        #define _LARGEFILE64_SOURCE
    #endif
    #ifndef _FILE_OFFSET_BIT
        #define _FILE_OFFSET_BIT 64
    #endif
#endif

#if (defined(_WIN32))
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif
#endif

#ifdef __APPLE__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <utime.h>
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "zip.h"

#ifdef _WIN32
    #define USEWIN32IOAPI
    #include "iowin32.h"
#endif

#define WRITEBUFFERSIZE (16384)
#define MAXFILENAME     (256)

PyObject* pyerr_msg = NULL;

extern PyObject *py_uncompress(PyObject *self, PyObject *args);

uLong filetime(const char *filename, tm_zip *tmzip, uLong *dostime)
{
    int ret = 0;
#ifdef _WIN32
    FILETIME ftLocal;
    HANDLE hFind;
    WIN32_FIND_DATAA ff32;

    hFind = FindFirstFileA(filename, &ff32);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        FileTimeToLocalFileTime(&(ff32.ftLastWriteTime), &ftLocal);
        FileTimeToDosDateTime(&ftLocal,((LPWORD)dostime)+1,((LPWORD)dostime)+0);
        FindClose(hFind);
        ret = 1;
    }
#else
#if defined unix || defined __APPLE__
    struct stat s = {0};
    struct tm* filedate;
    time_t tm_t = 0;

    if (strcmp(filename,"-") != 0)
    {
        char name[MAXFILENAME+1];
        int len = strlen(filename);
        if (len > MAXFILENAME)
            len = MAXFILENAME;

        strncpy(name, filename, MAXFILENAME - 1);
        name[MAXFILENAME] = 0;

        if (name[len - 1] == '/')
            name[len - 1] = 0;

        /* not all systems allow stat'ing a file with / appended */
        if (stat(name,&s) == 0)
        {
            tm_t = s.st_mtime;
            ret = 1;
        }
    }

    filedate = localtime(&tm_t);

    tmzip->tm_sec  = filedate->tm_sec;
    tmzip->tm_min  = filedate->tm_min;
    tmzip->tm_hour = filedate->tm_hour;
    tmzip->tm_mday = filedate->tm_mday;
    tmzip->tm_mon  = filedate->tm_mon ;
    tmzip->tm_year = filedate->tm_year;
#endif
#endif
    return ret;
}

int check_file_exists(const char* filename)
{
    FILE* ftestexist = FOPEN_FUNC(filename,"rb");
    if (ftestexist == NULL)
        return 0;
    fclose(ftestexist);
    return 1;
}

int is_large_file(const char* filename)
{
    ZPOS64_T pos = 0;
    FILE* pFile = FOPEN_FUNC(filename, "rb");

    if (pFile == NULL)
        return 0;

    FSEEKO_FUNC(pFile, 0, SEEK_END);
    pos = FTELLO_FUNC(pFile);
    fclose(pFile);

    // printf("File : %s is %lld bytes\n", filename, pos);

    return (pos >= 0xffffffff);
}

/* Calculate the CRC32 of a file, because to encrypt a file, we need known the CRC32 of the file before */
int get_file_crc(const char* filenameinzip, void *buf, unsigned long size_buf, unsigned long* result_crc)
{
    FILE *fin = NULL;
    unsigned long calculate_crc = 0;
    unsigned long size_read = 0;
    unsigned long total_read = 0;
    int err = ZIP_OK;

    fin = FOPEN_FUNC(filenameinzip,"rb");

    if (fin == NULL) {
        pyerr_msg = PyErr_Format(PyExc_IOError, "error in opening %s", filenameinzip);
        err = ZIP_ERRNO;
    }
    else
    {
        do
        {
            size_read = (int)fread(buf,1,size_buf,fin);

            if ((size_read < size_buf) && (feof(fin) == 0))
            {
                pyerr_msg = PyErr_Format(PyExc_IOError, "error in reading %s", filenameinzip);
                err = ZIP_ERRNO;
            }

            if (size_read > 0)
                calculate_crc = crc32(calculate_crc,buf,size_read);

            total_read += size_read;
        }
        while ((err == ZIP_OK) && (size_read > 0));
    }

    if (fin)
        fclose(fin);

    // printf("file %s crc %lx\n", filenameinzip, calculate_crc);

    *result_crc = calculate_crc;
    return err;
}

int _compress(const char** srcs, int src_num, const char** srcspath, int srcpath_num,
	      const char* dst, int level, const char* password, int exclude_path, PyObject* progress)
{
    zipFile zf = NULL;
    int size_buf = WRITEBUFFERSIZE;
    int opt_overwrite = APPEND_STATUS_CREATE;
    int err = ZIP_OK;
    int errclose = 0;
    int i;
#ifdef USEWIN32IOAPI
    zlib_filefunc64_def ffunc = {0};
#endif

    void* buf = NULL;
    buf = (void*)malloc(size_buf);
    if (buf == NULL)
    {
        pyerr_msg = PyErr_Format(PyExc_MemoryError, "could not allocate memory");
        return ZIP_ERRNO;
    }

    if (srcpath_num > 0)
        assert(src_num == srcpath_num);

#ifdef USEWIN32IOAPI
    fill_win32_filefunc64A(&ffunc);
    zf = zipOpen2_64(dst, opt_overwrite, NULL, &ffunc);
#else
    zf = zipOpen64(dst, opt_overwrite);
#endif

    if (zf == NULL)
    {
        pyerr_msg = PyErr_Format(PyExc_IOError, "error opening %s", dst);
        err = ZIP_ERRNO;
    }

    for (i = 0; i < src_num && (err == ZIP_OK); i++) {

        FILE *fin = NULL;
        int size_read = 0;
        const char* filenameinzip = srcs[i];
        const char* filepathnameinzip;
        const char *savefilenameinzip;
        const char *savefilepathnameinzip = NULL;
        char *fullpathfileinzip = NULL;
        unsigned long crcFile = 0;
        int zip64 = 0;

        zip_fileinfo zi;
        memset(&zi, 0, sizeof(zip_fileinfo));

        if (srcpath_num > 0)
            filepathnameinzip = srcspath[i];

        /* Get information about the file on disk so we can store it in zip */
        filetime(filenameinzip, &zi.tmz_date, &zi.dosDate);

        if ((password != NULL) && (err == ZIP_OK))
            err = get_file_crc(filenameinzip, buf, size_buf, &crcFile);

        zip64 = is_large_file(filenameinzip);

        /* Construct the filename that our file will be stored in the zip as. 
           The path name saved, should not include a leading slash. 
           If it did, windows/xp and dynazip couldn't read the zip file. */

        savefilenameinzip = filenameinzip;
        while (savefilenameinzip[0] == '\\' || savefilenameinzip[0] == '/')
            savefilenameinzip++;

        if (srcpath_num > 0) {
            savefilepathnameinzip = filepathnameinzip;
            while (savefilepathnameinzip[0] == '\\' || savefilepathnameinzip[0] == '/')
                savefilepathnameinzip++;
        }

        /* Should the file be stored with any path info at all? */
        if (exclude_path)
        {
            const char *tmpptr = NULL;
            const char *lastslash = NULL;

            for (tmpptr = savefilenameinzip; *tmpptr; tmpptr++)
            {
                if (*tmpptr == '\\' || *tmpptr == '/')
                    lastslash = tmpptr;
            }

            if (lastslash != NULL)
                savefilenameinzip = lastslash + 1; // base filename follows last slash.

            if (srcpath_num > 0) {
                /* prepend savefilepathnameinzip for each savefilenameinzip */
                const char * slash;
#if (defined(_WIN32))
                const char default_slash = '\\';
#else
                const char default_slash = '/';
#endif
                size_t extra_len = 0;
                size_t filename_len = strlen(savefilenameinzip);
                size_t filepathname_len = strlen(savefilepathnameinzip);

                /* look for slash used in filepath */
                slash = strchr(savefilepathnameinzip, '/');
                if (slash == NULL) {
                    slash = strchr(savefilepathnameinzip, '\\');
                    if (slash == NULL) {
                        // no slash found.. use default
                        slash = &default_slash;
                    }
                }

                /* NOTE:
                    don't add slash if filepathname_len is zero (prefix is an empty string)
                    and avoid memory access violation */
                if (filepathname_len > 0) {
                    if (savefilepathnameinzip[filepathname_len-1] != *slash)
                        extra_len = 1;
                }
                /* allocate buffer */
                fullpathfileinzip = (char *)malloc(filename_len + filepathname_len + extra_len + 1);
                if (fullpathfileinzip == NULL) {
                    free(buf);
                    pyerr_msg = PyErr_Format(PyExc_MemoryError, "error allocating memory on minizip compress");
                    return ZIP_INTERNALERROR;
                }

                strncpy(fullpathfileinzip, savefilepathnameinzip, filepathname_len);
                if (extra_len)
                    fullpathfileinzip[filepathname_len] = *slash;
                strncpy(fullpathfileinzip + filepathname_len + extra_len, savefilenameinzip, filename_len);
                /* terminate string */
                fullpathfileinzip[filename_len + filepathname_len + extra_len] = '\0';

                /* set pointer */
                savefilenameinzip = fullpathfileinzip;
            }
        }

        /* Add to zip file */
        err = zipOpenNewFileInZip3_64(zf, savefilenameinzip, &zi,
                    NULL, 0, NULL, 0, NULL /* comment*/,
                    (level != 0) ? Z_DEFLATED : 0, level, 0,
                    /* -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, */
                    -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                    password, crcFile, zip64);

        if (err != ZIP_OK) {
            pyerr_msg = PyErr_Format(PyExc_IOError, "error in opening %s in zipfile (%d)", filenameinzip, err);
            err = ZIP_ERRNO;
        }
        else
        {
            fin = FOPEN_FUNC(filenameinzip, "rb");
            if (fin == NULL)
            {
                pyerr_msg = PyErr_Format(PyExc_IOError, "error in opening %s for reading", filenameinzip);
                err = ZIP_ERRNO;
            }
        }

        if (err == ZIP_OK)
        {
            /* Read contents of file and write it to zip */
            do
            {
                size_read = (int)fread(buf, 1, size_buf, fin);
                if ((size_read < size_buf) && (feof(fin) == 0))
                {
                    pyerr_msg = PyErr_Format(PyExc_IOError, "error in reading %s", filenameinzip);
                    err = ZIP_ERRNO;
                }

                if (0 < size_read)
                {
                    err = zipWriteInFileInZip(zf, buf, size_read);
                    if (err < 0) {
                        pyerr_msg = PyErr_Format(PyExc_IOError, "error in writing %s in the zipfile (%d)", filenameinzip, err);
                        err = ZIP_ERRNO;
                    }
                }
            }
            while ((err == ZIP_OK) && (size_read > 0));
        }

        if (fin)
            fclose(fin);

        if (err == ZIP_OK)
        {
            err = zipCloseFileInZip(zf);
            if (err != ZIP_OK) {
                pyerr_msg = PyErr_Format(PyExc_IOError, "error in closing %s in the zipfile (%d)", filenameinzip, err);
                err = ZIP_ERRNO;
            }
        }

        if (progress != NULL)
        {
	    PyObject* args = Py_BuildValue("(I)", i + 1);
	    PyObject* result = PyObject_CallObject(progress, args);
	    if (PyErr_Occurred()) // Ignore errors in the callback, don't want them to crash this c module
	    {
                PyErr_Clear();
	    }
	    Py_XDECREF(result);
	    Py_XDECREF(args);
        }

        if(srcpath_num > 0 && fullpathfileinzip)
            free(fullpathfileinzip);
    }

    errclose = zipClose(zf, NULL);
    if (errclose != ZIP_OK) {
        pyerr_msg = PyErr_Format(PyExc_IOError, "error in closing %s (%d)", dst, errclose);
        err = ZIP_ERRNO;
    }

    free(buf);

    return err;
}

static PyObject *py_compress(PyObject *self, PyObject *args)
{
    Py_ssize_t src_len, srcpath_len, dst_len, pass_len;
    int level, res;
    const char * src;
    const char * srcpath;
    const char * dst;
    const char * pass;

    if (!PyArg_ParseTuple(args, "z#z#z#z#i",
                                &src, &src_len,
                                &srcpath, &srcpath_len,
                                &dst, &dst_len,
                                &pass, &pass_len,
                                &level)) {
        return PyErr_Format(PyExc_ValueError, "expected arguments are compress(srcfile, prefix, zipfile, password, compress_level)");
    }

    if (src_len < 1) {
        return PyErr_Format(PyExc_ValueError, "compress src file is None");
    }

    if (srcpath_len > 0) {
        srcpath_len = 1;
    }

    if (dst_len < 1) {
        return PyErr_Format(PyExc_ValueError, "compress dst file is None");
    }

    if (level < 1 || 9 < level) {
        level = Z_DEFAULT_COMPRESSION;
    }

    if (pass_len < 1) {
        pass = NULL;
    }

    res = _compress(&src, 1, &srcpath, srcpath_len, dst, level, pass, 1, NULL);

    if (res != ZIP_OK) {
        return pyerr_msg;
    }

    Py_RETURN_NONE;
}

static PyObject *py_compress_multiple(PyObject *self, PyObject *args)
{
    int i;
    int src_len, srcpath_len, level, res;
    Py_ssize_t dst_len, pass_len;
    PyObject * src, * srcpath;
    char ** srcs, ** srcspath = NULL;
    const char * dst;
    const char * pass;

    PyObject * str_obj; /* the list of strings */
    PyObject * strpath_obj; /* the list of path strings */

    PyObject * progress_cb_obj = NULL;

    if (!PyArg_ParseTuple(args, "O!O!z#z#i|O",
			  &PyList_Type, &src,
			  &PyList_Type, &srcpath,
			  &dst, &dst_len,
			  &pass, &pass_len,
			  &level,
			  &progress_cb_obj)) {
        return PyErr_Format(PyExc_ValueError,
                        "expected arguments are "
                        "compress_multiple(srcfiles, prefixs, zipfile, password, compress_level, progress)");
    }

    src_len = PyList_Size(src);

    if (src_len < 1) {
        return PyErr_Format(PyExc_ValueError, "compress src file is None");
    }

    srcpath_len = PyList_Size(srcpath);
    if (srcpath_len < 1) {
        srcpath = NULL;
    } else if (srcpath_len != src_len) {
        return PyErr_Format(PyExc_ValueError, "compress src file list has different length "
                                              "than src file path list");
    }

    if (dst_len < 1) {
        return PyErr_Format(PyExc_ValueError, "compress dst file is None");
    }

    if (level < 1 || 9 < level) {
        level = Z_DEFAULT_COMPRESSION;
    }

    if (pass_len < 1) {
        pass = NULL;
    }

    if (progress_cb_obj != NULL) {
        if (!PyFunction_Check(progress_cb_obj)) {
            return PyErr_Format(PyExc_ValueError, "progress must be function or None");
        }
    }

    for (i = 0; i < src_len; i++) {
        str_obj = PyList_GetItem(src, i);
#if PY_MAJOR_VERSION >= 3
        if (!PyUnicode_Check(str_obj))
#else
        if (!PyString_Check(str_obj) && !PyUnicode_Check(str_obj))
#endif
        {
            return PyErr_Format(PyExc_ValueError, "[src] elements must be strings");
        }
    }

    srcs = (char **)malloc(src_len * sizeof(char *));

    if (srcs == NULL) {
        return PyErr_NoMemory();
    }

    for (i = 0; i < src_len; i++) {
        str_obj = PyList_GetItem(src, i);
#if PY_MAJOR_VERSION >= 3
        srcs[i] = PyUnicode_AsUTF8(str_obj);
#else
        srcs[i] = PyString_AsString(str_obj);
#endif
    }

    if (srcpath) {
        for (i = 0; i < srcpath_len; i++) {
                strpath_obj = PyList_GetItem(srcpath, i);
#if PY_MAJOR_VERSION >= 3
                if (!PyUnicode_Check(strpath_obj))
#else
                if (!PyString_Check(strpath_obj) && !PyUnicode_Check(strpath_obj))
#endif
                {
                        return PyErr_Format(PyExc_ValueError, "[srcpath] elements must be strings");
                }
        }

        srcspath = (char **)malloc(srcpath_len * sizeof(char *));
        if (srcspath == NULL) {
                return PyErr_NoMemory();
        }

        for (i = 0; i < srcpath_len; i++) {
                strpath_obj = PyList_GetItem(srcpath, i);
#if PY_MAJOR_VERSION >= 3
                srcspath[i] = PyUnicode_AsUTF8(strpath_obj);
#else
                srcspath[i] = PyString_AsString(strpath_obj);
#endif
        }
    }

    res = _compress((const char **)srcs, src_len,
		    (const char **)srcspath, srcpath_len,
		    dst, level, pass, 1, progress_cb_obj);

    // cleanup free up heap allocated memory
    free(srcs);

    if (srcspath)
        free(srcspath);

    if (res != ZIP_OK) {
        return pyerr_msg;
    }

    Py_RETURN_NONE;
}

static char ext_doc[] = "C extention for encrypted zip compress/uncompress.\n";

static PyMethodDef py_minizip_methods[] = {
 {
        "compress",
        py_compress, METH_VARARGS,
        "compress(srcfile, prefix, zipfile, password, compress_level)\n"
        "--\n\n"
        "make compressed file.\n"
        "\n"
        ":param srcfile: path to source file. (str)\n"
        ":param prefix: prefix path for the file in the zip. (str)\n"
        ":param zipfile: path of the zip file to be created. (str)\n"
        ":param password: password of the zip, or None for no-password zip. (str)\n"
        ":param compress_level: compress level between 1(faster) to 9(more compress). (int)\n"
    },
    {
        "compress_multiple",
        py_compress_multiple, METH_VARARGS,
        "compress_multiple(srcfiles, prefixs, zipfile, password, compress_level, progress= None)\n"
        "--\n\n"
        "make compressed file with many files.\n"
        "\n"
        ":param srcfiles: list of path to source file. (list of str)\n"
        ":param prefixs: list fo prefix path for the file in the zip, or [] to left them in root directory. (list of str)\n"
        ":param zipfile: path of the zip file to be created. (str)\n"
        ":param password: password of the zip, or None for no-password zip. (str)\n"
        ":param compress_level: compress level between 1(faster) to 9(more compress). (int)\n"
        ":param progress: function to be called during processing which takes one argument, the count of how many files have been compressed. (callable)\n"
    },
    {
        "uncompress",
        py_uncompress, METH_VARARGS,
        "uncompress(zipfile, password, dir, extract_withoutpath)\n"
        "--\n\n"
        "extract compressed file.\n"
        "\n"
        ":param zipfile: path of the zip file to be extracted. (str)\n"
        ":param password: password of the zip, or None for no-password zip. (str)\n"
        ":param dir: path to extract files, or None to extract in CWD. (str)\n"
        ":param extract_withoutpath: extract file without path; 0 = use path in the zip, otherwise all files would be placed in the targeted directory. (int)\n"
    },
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "pyminizip",
        ext_doc,
        -1,
        py_minizip_methods,
        NULL,
        NULL,
        NULL,
        NULL
};

PyObject* PyInit_pyminizip(void) {
    PyObject *module = PyModule_Create(&moduledef);
    return module;
}

#else
void initpyminizip(void) {
    Py_InitModule3("pyminizip", py_minizip_methods, ext_doc);
}
#endif
