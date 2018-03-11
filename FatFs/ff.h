#pragma once
//{{{  integer
#include "stdint.h"

// These types MUST be 16-bit or 32-bit
typedef int            INT;
typedef unsigned int   UINT;

// This type MUST be 8-bit
typedef unsigned char  BYTE;

// These types MUST be 16-bit
typedef short          SHORT;
typedef unsigned short WORD;
typedef unsigned short WCHAR;

// These types MUST be 32-bit
typedef long           LONG;
typedef unsigned long  DWORD;

// This type MUST be 64-bit (Remove this for ANSI C (C89) compatibility)
typedef unsigned long long QWORD;
//}}}
//{{{  ffconf
#define _FFCONF 68300 /* Revision ID */

#define _FS_READONLY  0
#define _FS_MINIMIZE  0
#define _USE_STRFUNC  1
#define _USE_FIND   1
#define _USE_MKFS   1
#define _USE_FASTSEEK 1
#define _USE_EXPAND   0
#define _USE_CHMOD    1
#define _USE_LABEL    1
#define _USE_FORWARD  0
#define _CODE_PAGE  850
#define _USE_LFN  3
#define _MAX_LFN  255
#define _LFN_UNICODE  0
#define _STRF_ENCODE  3
#define _FS_RPATH 2
#define _VOLUMES  2
#define _STR_VOLUME_ID  0
#define _VOLUME_STRS  "RAM","NAND","CF","SD","SD2","USB","USB2","USB3"
#define _MULTI_PARTITION  0
#define _MIN_SS   512
#define _MAX_SS   4096
#define _USE_TRIM 0
#define _FS_NOFSINFO  0
#define _FS_TINY  0
#define _FS_EXFAT 1
#define _FS_NORTC 0
#define _NORTC_MON  1
#define _NORTC_MDAY 1
#define _NORTC_YEAR 2016
#define _FS_LOCK  2

#define _FS_REENTRANT 0
#define _SYNC_t         int
//}}}
//{{{
#ifdef __cplusplus
extern "C" {
#endif
//}}}

/* Definitions of volume management */
typedef struct {
  BYTE pd;  /* Physical drive number */
  BYTE pt;  /* Partition: 0:Auto detect, 1-4:Forced partition) */
  } PARTITION;
extern PARTITION VolToPart[]; /* Volume - Partition resolution table */

/* Type of path name strings on FatFs API */
#if _LFN_UNICODE      /* Unicode (UTF-16) string */
  #if _USE_LFN == 0
    #error _LFN_UNICODE must be 0 at non-LFN cfg.
  #endif
  #ifndef _INC_TCHAR
    typedef WCHAR TCHAR;
    #define _T(x) L ## x
    #define _TEXT(x) L ## x
  #endif
#else           /* ANSI/OEM string */
  #ifndef _INC_TCHAR
  typedef char TCHAR;
  #define _T(x) x
  #define _TEXT(x) x
  #endif
#endif

typedef QWORD FSIZE_t;

/* File system object structure (FATFS) */
typedef struct {
  BYTE  fs_type;    /* File system type (0:N/A) */
  BYTE  drv;        /* Physical drive number */
  BYTE  n_fats;     /* Number of FATs (1 or 2) */
  BYTE  wflag;      /* win[] flag (b0:dirty) */
  BYTE  fsi_flag;   /* FSINFO flags (b7:disabled, b0:dirty) */
  WORD  id;         /* File system mount ID */
  WORD  n_rootdir;  /* Number of root directory entries (FAT12/16) */
  WORD  csize;      /* Cluster size [sectors] */
  WORD  ssize;      /* Sector size (512, 1024, 2048 or 4096) */
  WCHAR*  lfnbuf;   /* LFN working buffer */
  BYTE* dirbuf;     /* Directory entry block scratchpad buffer */
  _SYNC_t sobj;     /* Identifier of sync object */
  DWORD last_clst;  /* Last allocated cluster */
  DWORD free_clst;  /* Number of free clusters */
  DWORD cdir;       /* Current directory start cluster (0:root) */
  DWORD cdc_scl;    /* Containing directory start cluster (invalid when cdir is 0) */
  DWORD cdc_size;   /* b31-b8:Size of containing directory, b7-b0: Chain status */
  DWORD cdc_ofs;    /* Offset in the containing directory (invalid when cdir is 0) */
  DWORD n_fatent;   /* Number of FAT entries (number of clusters + 2) */
  DWORD fsize;      /* Size of an FAT [sectors] */
  DWORD volbase;    /* Volume base sector */
  DWORD fatbase;    /* FAT base sector */
  DWORD dirbase;    /* Root directory base sector/cluster */
  DWORD database;   /* Data base sector */
  DWORD winsect;    /* Current sector appearing in the win[] */
  BYTE  win[_MAX_SS]; /* Disk access window for Directory, FAT (and file data at tiny cfg) */
  } FATFS;

/* Object ID and allocation information (_FDID) */
typedef struct {
  FATFS*  fs;      /* Pointer to the owner file system object */
  WORD  id;        /* Owner file system mount ID */
  BYTE  attr;      /* Object attribute */
  BYTE  stat;      /* Object chain status (b1-0: =0:not contiguous, =2:contiguous (no data on FAT), =3:flagmented in this session, b2:sub-directory stretched) */
  DWORD sclust;    /* Object start cluster (0:no cluster or root directory) */
  FSIZE_t objsize; /* Object size (valid when sclust != 0) */
  DWORD n_cont;    /* Size of first fragment, clusters - 1 (valid when stat == 3) */
  DWORD n_frag;    /* Size of last fragment needs to be written (valid when not zero) */
  DWORD c_scl;     /* Containing directory start cluster (valid when sclust != 0) */
  DWORD c_size;    /* b31-b8:Size of containing directory, b7-b0: Chain status (valid when c_scl != 0) */
  DWORD c_ofs;     /* Offset in the containing directory (valid when sclust != 0 and non-directory object) */
  UINT  lockid;    /* File lock ID origin from 1 (index of file semaphore table Files[]) */
  } _FDID;

/* File object structure (FIL) */
typedef struct {
  _FDID obj;      /* Object identifier (must be the 1st member to detect invalid object pointer) */
  BYTE  flag;     /* File status flags */
  BYTE  err;      /* Abort flag (error code) */
  FSIZE_t fptr;   /* File read/write pointer (Zeroed on file open) */
  DWORD clust;    /* Current cluster of fpter (invalid when fptr is 0) */
  DWORD sect;     /* Sector number appearing in buf[] (0:invalid) */
  DWORD dir_sect; /* Sector number containing the directory entry */
  BYTE* dir_ptr;  /* Pointer to the directory entry in the win[] */
  DWORD*  cltbl;  /* Pointer to the cluster link map table (nulled on open, set by application) */
  BYTE  buf[_MAX_SS]; /* File private data read/write window */
  } FIL;

/* Directory object structure (DIR) */
typedef struct {
  _FDID obj;      /* Object identifier */
  DWORD dptr;     /* Current read/write offset */
  DWORD clust;    /* Current cluster */
  DWORD sect;     /* Current sector (0:Read operation has terminated) */
  BYTE* dir;      /* Pointer to the directory item in the win[] */
  BYTE  fn[12];   /* SFN (in/out) {body[8],ext[3],status[1]} */
  DWORD blk_ofs;  /* Offset of current entry block being processed (0xFFFFFFFF:Invalid) */
  const TCHAR* pat;  /* Pointer to the name matching pattern */
  } DIR;

/* File information structure (FILINFO) */
typedef struct {
  FSIZE_t fsize;    /* File size */
  WORD  fdate;      /* Modified date */
  WORD  ftime;      /* Modified time */
  BYTE  fattrib;    /* File attribute */
  TCHAR altname[13];   /* Alternative file name */
  TCHAR fname[_MAX_LFN + 1];  /* Primary file name */
  } FILINFO;

/* File function return code (FRESULT) */
typedef enum {
  FR_OK = 0,        /* (0) Succeeded */
  FR_DISK_ERR,      /* (1) A hard error occurred in the low level disk I/O layer */
  FR_INT_ERR,       /* (2) Assertion failed */
  FR_NOT_READY,     /* (3) The physical drive cannot work */
  FR_NO_FILE,       /* (4) Could not find the file */
  FR_NO_PATH,       /* (5) Could not find the path */
  FR_INVALID_NAME,  /* (6) The path name format is invalid */
  FR_DENIED,        /* (7) Access denied due to prohibited access or directory full */
  FR_EXIST,         /* (8) Access denied due to prohibited access */
  FR_INVALID_OBJECT,   /* (9) The file/directory object is invalid */
  FR_WRITE_PROTECTED,  /* (10) The physical drive is write protected */
  FR_INVALID_DRIVE,    /* (11) The logical drive number is invalid */
  FR_NOT_ENABLED,      /* (12) The volume has no work area */
  FR_NO_FILESYSTEM,    /* (13) There is no valid FAT volume */
  FR_MKFS_ABORTED,     /* (14) The f_mkfs() aborted due to any problem */
  FR_TIMEOUT,          /* (15) Could not get a grant to access the volume within defined period */
  FR_LOCKED,           /* (16) The operation is rejected according to the file sharing policy */
  FR_NOT_ENOUGH_CORE,  /* (17) LFN working buffer could not be allocated */
  FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > _FS_LOCK */
  FR_INVALID_PARAMETER    /* (19) Given parameter is invalid */
  } FRESULT;

/* FatFs module application interface                           */
FRESULT f_open (FIL* fp, const TCHAR* path, BYTE mode);       /* Open or create a file */
FRESULT f_close (FIL* fp);                      /* Close an open file object */
FRESULT f_read (FIL* fp, void* buff, UINT btr, UINT* br);     /* Read data from the file */
FRESULT f_write (FIL* fp, const void* buff, UINT btw, UINT* bw);  /* Write data to the file */
FRESULT f_lseek (FIL* fp, FSIZE_t ofs);               /* Move file pointer of the file object */
FRESULT f_truncate (FIL* fp);                   /* Truncate the file */
FRESULT f_sync (FIL* fp);                     /* Flush cached data of the writing file */
FRESULT f_opendir (DIR* dp, const TCHAR* path);           /* Open a directory */
FRESULT f_closedir (DIR* dp);                   /* Close an open directory */
FRESULT f_readdir (DIR* dp, FILINFO* fno);              /* Read a directory item */
FRESULT f_findfirst (DIR* dp, FILINFO* fno, const TCHAR* path, const TCHAR* pattern); /* Find first file */
FRESULT f_findnext (DIR* dp, FILINFO* fno);             /* Find next file */
FRESULT f_mkdir (const TCHAR* path);                /* Create a sub directory */
FRESULT f_unlink (const TCHAR* path);               /* Delete an existing file or directory */
FRESULT f_rename (const TCHAR* path_old, const TCHAR* path_new);  /* Rename/Move a file or directory */
FRESULT f_stat (const TCHAR* path, FILINFO* fno);         /* Get file status */
FRESULT f_chmod (const TCHAR* path, BYTE attr, BYTE mask);      /* Change attribute of a file/dir */
FRESULT f_utime (const TCHAR* path, const FILINFO* fno);      /* Change timestamp of a file/dir */
FRESULT f_chdir (const TCHAR* path);                /* Change current directory */
FRESULT f_chdrive (const TCHAR* path);                /* Change current drive */
FRESULT f_getcwd (TCHAR* buff, UINT len);             /* Get current directory */
FRESULT f_getfree (const TCHAR* path, DWORD* nclst, FATFS** fatfs); /* Get number of free clusters on the drive */
FRESULT f_getlabel (const TCHAR* path, TCHAR* label, DWORD* vsn); /* Get volume label */
FRESULT f_setlabel (const TCHAR* label);              /* Set volume label */
FRESULT f_forward (FIL* fp, UINT(*func)(const BYTE*,UINT), UINT btf, UINT* bf); /* Forward data to the stream */
FRESULT f_expand (FIL* fp, FSIZE_t szf, BYTE opt);          /* Allocate a contiguous block to the file */
FRESULT f_mount (FATFS* fs, const TCHAR* path, BYTE opt);     /* Mount/Unmount a logical drive */
FRESULT f_mkfs (const TCHAR* path, BYTE opt, DWORD au, void* work, UINT len); /* Create a FAT volume */
FRESULT f_fdisk (BYTE pdrv, const DWORD* szt, void* work);      /* Divide a physical drive into some partitions */
int f_putc (TCHAR c, FIL* fp);                    /* Put a character to the file */
int f_puts (const TCHAR* str, FIL* cp);               /* Put a string to the file */
int f_printf (FIL* fp, const TCHAR* str, ...);            /* Put a formatted string to the file */
TCHAR* f_gets (TCHAR* buff, int len, FIL* fp);            /* Get a string from the file */

#define f_eof(fp) ((int)((fp)->fptr == (fp)->obj.objsize))
#define f_error(fp) ((fp)->err)
#define f_tell(fp) ((fp)->fptr)
#define f_size(fp) ((fp)->obj.objsize)
#define f_rewind(fp) f_lseek((fp), 0)
#define f_rewinddir(dp) f_readdir((dp), 0)
#define f_rmdir(path) f_unlink(path)

#ifndef EOF
  #define EOF (-1)
#endif

/* Additional user defined functions                            */
/* RTC function */
DWORD get_fattime (void);

/* Unicode support functions */
WCHAR ff_convert (WCHAR chr, UINT dir); /* OEM-Unicode bidirectional conversion */
WCHAR ff_wtoupper (WCHAR chr);      /* Unicode upper-case conversion */
void* ff_memalloc (UINT msize);     /* Allocate memory block */
void ff_memfree (void* mblock);     /* Free memory block */

/* Sync functions */
#if _FS_REENTRANT
  int ff_cre_syncobj (BYTE vol, _SYNC_t* sobj); /* Create a sync object */
  int ff_req_grant (_SYNC_t sobj);        /* Lock sync object */
  void ff_rel_grant (_SYNC_t sobj);       /* Unlock sync object */
  int ff_del_syncobj (_SYNC_t sobj);        /* Delete a sync object */
#endif

/* Flags and offset address                                     */
/* File access mode and open method flags (3rd argument of f_open) */
#define FA_READ       0x01
#define FA_WRITE      0x02
#define FA_OPEN_EXISTING  0x00
#define FA_CREATE_NEW   0x04
#define FA_CREATE_ALWAYS  0x08
#define FA_OPEN_ALWAYS    0x10
#define FA_OPEN_APPEND    0x30

/* Fast seek controls (2nd argument of f_lseek) */
#define CREATE_LINKMAP  ((FSIZE_t)0 - 1)

/* Format options (2nd argument of f_mkfs) */
#define FM_FAT    0x01
#define FM_FAT32  0x02
#define FM_EXFAT  0x04
#define FM_ANY    0x07
#define FM_SFD    0x08

/* Filesystem type (FATFS.fs_type) */
#define FS_FAT12  1
#define FS_FAT16  2
#define FS_FAT32  3
#define FS_EXFAT  4

/* File attribute bits for directory entry (FILINFO.fattrib) */
#define AM_RDO  0x01  /* Read only */
#define AM_HID  0x02  /* Hidden */
#define AM_SYS  0x04  /* System */
#define AM_DIR  0x10  /* Directory */
#define AM_ARC  0x20  /* Archive */

//{{{
#ifdef __cplusplus
}
#endif
//}}}
