/*
   Original is miniunz.c

   miniunz.c
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
# include <sys/stat.h>
#endif

#ifdef _WIN32
#define MKDIR(d) _mkdir(d)
#define CHDIR(d) _chdir(d)
#else
#define MKDIR(d) mkdir(d, 0775)
#define CHDIR(d) chdir(d)
#endif

#include "unzip.h"

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME     (256)

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif

PyObject* pyerr_msg_unz = NULL;

void change_file_date(const char *filename, uLong dosdate, tm_unz tmu_date)
{
#ifdef _WIN32
    HANDLE hFile;
    FILETIME ftm, ftLocal, ftCreate, ftLastAcc, ftLastWrite;

    hFile = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        GetFileTime(hFile, &ftCreate, &ftLastAcc, &ftLastWrite);
        DosDateTimeToFileTime((WORD)(dosdate>>16),(WORD)dosdate, &ftLocal);
        LocalFileTimeToFileTime(&ftLocal, &ftm);
        SetFileTime(hFile, &ftm, &ftLastAcc, &ftm);
        CloseHandle(hFile);
    }
#else
#if defined unix || defined __APPLE__
    struct utimbuf ut;
    struct tm newdate;

    newdate.tm_sec = tmu_date.tm_sec;
    newdate.tm_min = tmu_date.tm_min;
    newdate.tm_hour = tmu_date.tm_hour;
    newdate.tm_mday = tmu_date.tm_mday;
    newdate.tm_mon = tmu_date.tm_mon;
    if (tmu_date.tm_year > 1900)
        newdate.tm_year = tmu_date.tm_year - 1900;
    else
        newdate.tm_year = tmu_date.tm_year ;
    newdate.tm_isdst = -1;

    ut.actime = ut.modtime = mktime(&newdate);
    utime(filename,&ut);
#endif
#endif
}

extern int check_file_exists(const char* filename);

int makedir(const char *newdir)
{
    char *buffer = NULL;
    char *p = NULL;
    int len = (int)strlen(newdir);

    if (len <= 0)
        return 0;

    buffer = (char*)malloc(len+1);
    if (buffer == NULL)
    {
        pyerr_msg_unz = PyErr_Format(PyExc_MemoryError, "error allocating memory");
        return UNZ_INTERNALERROR;
    }

    strcpy(buffer, newdir);

    if (buffer[len-1] == '/') 
        buffer[len-1] = 0;

    if (MKDIR(buffer) == 0)
    {
        free(buffer);
        return 1;
    }

    p = buffer + 1;
    while (1)
    {
        char hold;
        while(*p && *p != '\\' && *p != '/')
            p++;
        hold = *p;
        *p = 0;

        if ((MKDIR(buffer) == -1) && (errno == ENOENT))
        {
            free(buffer);
            return 0;
        }

        if (hold == 0)
            break;

        *p++ = hold;
    }

    free(buffer);
    return 1;
}

int do_extract_currentfile(unzFile uf, const int* popt_extract_without_path, int* popt_overwrite, const char *password)
{
    char filename_inzip[256];
    char* filename_withoutpath;
    char* p;
    int err = UNZ_OK;
    FILE *fout = NULL;
    void* buf;
    uInt size_buf;
    unz_file_info64 file_info;

    const char* write_filename = NULL;
    int skip = 0;

    err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
    {
        pyerr_msg_unz = PyErr_Format(PyExc_Exception, "error %d with zipfile in unzGetCurrentFileInfo", err);
        return err;
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void*)malloc(size_buf);
    if (buf == NULL)
    {
        pyerr_msg_unz = PyErr_Format(PyExc_MemoryError, "error allocating memory");
        return UNZ_INTERNALERROR;
    }

    p = filename_withoutpath = filename_inzip;
    while ((*p) != '\0')
    {
        if (((*p)=='/') || ((*p)=='\\'))
            filename_withoutpath = p+1;
        p++;
    }

    /* If zip entry is a directory then create it on disk */
    if ((*filename_withoutpath)=='\0')
    {
        if ((*popt_extract_without_path)==0)
        {
            MKDIR(filename_inzip);
        }
    }
    else
    {
        if ((*popt_extract_without_path)==0)
            write_filename = filename_inzip;
        else
            write_filename = filename_withoutpath;

        err = unzOpenCurrentFilePassword(uf, password);
        if (err != UNZ_OK) {
            pyerr_msg_unz = PyErr_Format(PyExc_Exception,
                                "error %d with zipfile in unzOpenCurrentFilePassword", err);
        }


        /* Create the file on disk so we can unzip to it */
        if ((skip == 0) && (err == UNZ_OK))
        {
            fout = FOPEN_FUNC(write_filename, "wb");
            /* Some zips don't contain directory alone before file */
            if ((fout==NULL) && ((*popt_extract_without_path)==0) &&
                                (filename_withoutpath!=(char*)filename_inzip))
            {
                char c = *(filename_withoutpath-1);
                *(filename_withoutpath-1) = 0;
                makedir(write_filename);
                *(filename_withoutpath-1) = c;
                fout = FOPEN_FUNC(write_filename, "wb");
            }
            if (fout == NULL) {
                pyerr_msg_unz = PyErr_Format(PyExc_IOError, "error opening %s", write_filename);
            }
        }

        /* Read from the zip, unzip to buffer, and write to disk */
        if (fout != NULL)
        {

            do
            {
                err = unzReadCurrentFile(uf, buf, size_buf);
                if (err < 0)
                {
                    pyerr_msg_unz = PyErr_Format(PyExc_Exception, 
                                        "error %d with zipfile in unzReadCurrentFile", err);
                    break;
                }
                if (err>0)
                    if (fwrite(buf, err, 1, fout) != 1)
                    {
                        pyerr_msg_unz = PyErr_Format(PyExc_IOError, "error %d in writing extracted file", errno);
                        err = UNZ_ERRNO;
                        break;
                    }
            }
            while (err > 0);
            if (fout)
                fclose(fout);

            /* Set the time of the file that has been unzipped */
            if (err == 0)
                change_file_date(write_filename,file_info.dosDate, file_info.tmu_date);
        }

        if (err==UNZ_OK)
        {
            err = unzCloseCurrentFile(uf);
            if (err != UNZ_OK) {
                pyerr_msg_unz = PyErr_Format(PyExc_Exception,
                                "error %d with zipfile in unzCloseCurrentFile", err);
            }
        }
        else
            unzCloseCurrentFile(uf);
    }

    free(buf);
    return err;
}

int do_extract_all(unzFile uf, int opt_extract_without_path, int opt_overwrite, const char *password)
{
    uLong i;
    unz_global_info64 gi;
    int err;
    
    err = unzGetGlobalInfo64(uf,&gi);
    if (err != UNZ_OK) {
        pyerr_msg_unz = PyErr_Format(PyExc_Exception,
                            "error %d with zipfile in unzGetGlobalInfo", err);
        return 1;
    }

    for (i=0;i<gi.number_entry;i++)
    {
        err = do_extract_currentfile(uf,&opt_extract_without_path,
                                          &opt_overwrite,
                                          password);
        if (err != UNZ_OK) {
            pyerr_msg_unz = PyErr_Format(PyExc_Exception,
                            "error %d with zipfile in do_extract_currentfile", err);
            return 1;
        }

        if ((i+1)<gi.number_entry)
        {
            err = unzGoToNextFile(uf);
            if (err != UNZ_OK)
            {
                pyerr_msg_unz = PyErr_Format(PyExc_Exception,
                            "error %d with zipfile in unzGoToNextFile", err);
                return 1;
            }
        }
    }

    return 0;

}

int do_extract_onefile(unzFile uf, const char* filename, int opt_extract_without_path, int opt_overwrite, 
    const char* password)
{
    if (unzLocateFile(uf,filename,CASESENSITIVITY) != UNZ_OK)
    {
        return 2;
    }

    if (do_extract_currentfile(uf,&opt_extract_without_path,
                                      &opt_overwrite,
                                      password) == UNZ_OK)
        return 0;
    else
        return 1;
}

/* original usage string for miniunz is:
   "Usage : miniunz [-e] [-x] [-v] [-l] [-o] [-p password] file.zip [file_to_extr.] [-d extractdir]\n\n" \
         "  -e  Extract without pathname (junk paths)\n" \
         "  -x  Extract with pathname\n" \
         "  -v  list files\n" \
         "  -l  list files\n" \
         "  -d  directory to extract into\n" \
         "  -o  overwrite files without prompting\n" \
         "  -p  extract crypted file using password\n\n");
*/
int _uncompress(const char* src, const char* password, const char *dirname,
                int extract_withoutpath, PyObject* progress)
{
    const char *filename_to_extract = NULL;
    int ret = 0;
    int extract = 1;  // extract always
    int overwrite = 1;  // overwrite always
    int extractdir = 0;

    unzFile uf = NULL;
#ifdef USEWIN32IOAPI
    zlib_filefunc64_def ffunc = {0};
    fill_win32_filefunc64A(&ffunc);
    uf = unzOpen2_64(src, &ffunc);
#else
    uf = unzOpen64(src);
#endif

    if (uf == NULL)
    {
        pyerr_msg_unz = PyErr_Format(PyExc_IOError, "error opening %s", src);
        return UNZ_ERRNO;
    }

    if (dirname != NULL)
        extractdir = 1;

    /* Process command line options */
    if (extract == 1)
    {
        if (extractdir && CHDIR(dirname))
        {
            pyerr_msg_unz = PyErr_Format(PyExc_OSError, "error changing into %s", dirname);
        }

        if (filename_to_extract == NULL)
            ret = do_extract_all(uf, extract_withoutpath, overwrite, password);
        // else
        //     ret = do_extract_onefile(uf, filename_to_extract, opt_do_extract_withoutpath, opt_overwrite, password);
    }

    // if (ret != UNZ_OK)
    //     pyerr_msg_unz = PyErr_Format(PyExc_Exception, "generic error");

    unzClose(uf);
    return ret;
}

PyObject *py_uncompress(PyObject *self, PyObject *args)
{
    Py_ssize_t src_len, pass_len, dir_len;
    int withoutpath, res;
    const char * src;
    const char * pass;
    const char * dir;

    if (!PyArg_ParseTuple(args, "z#z#z#i", &src, &src_len, &pass, &pass_len, &dir, &dir_len, &withoutpath)) {
        return PyErr_Format(PyExc_ValueError, "expected arguments are uncompress(zipfile, password, dir, extract_withoutpath)");
    }

    if (src_len < 1) {
        return PyErr_Format(PyExc_ValueError, "uncompress src file is None");
    }

    if (pass_len < 1) {
        pass = NULL;
    }

    if (dir_len < 1) {
        dir = NULL;
    }

    res = _uncompress(src, pass, dir, withoutpath, NULL);

    if (res != UNZ_OK) {
        return pyerr_msg_unz;
    }

    Py_RETURN_NONE;
}
