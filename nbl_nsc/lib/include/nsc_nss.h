// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NSC_NSS_H_
#define NSC_NSS_H_

// This is library for Non-Security Storage (NSS).
// - used for write/read data in non-secure world as fast as I can.
/**
 * Functions are like libc file, for examples: open/close/read/write and etc.
 */
#include <nbl_fwk/service.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "nsc_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////
// Below functions simulate open/close/write/read and etc of libc.

/**
 * Open a file.
 * \param path file's path relative to the data folder of saving NSS data.
 * \param flags operation flags: O_RDONLY/O_CREATE/...
 * If flags includes O_CREAT or O_TMPFILE, must add argument mode(type is mode_t=unsigned int).
 * \return -1: error, >=0: file descriptor
 */
int nbl_nsc_open(const char* nss_path, int flags, ...);

/**
 * Read data from a opened file.
 * \param fd file descriptor
 * \param buf data buffer
 * \param count buffer's size
 * \return -1: is error, >=0: is read size.
 */
int nbl_nsc_read(int fd, void* buf, size_t count);

/**
 * Write data to a opened file.
 * \param fd file descriptor
 * \param buf data's buffer
 * \param count buffer's size(bytes)
 * \return -1: is error, >=0: is written size.
 */
int nbl_nsc_write(int fd, const void* buf, size_t count);

/**
 * Close a file.
 * \param fd file descriptor.
 * \return -1: error, 0: ok.
 */
int nbl_nsc_close(int fd);

/**
 * Get file size of opened file.
 * \param fd file descriptor
 * \return <0: is err, >=0: file size.
 */
int nbl_nsc_fsize(int fd);

/**
 * Remove a file.
 * \param path file's path
 * \return -1: error, 0: OK
 */
int nbl_nsc_remove(const char* path);

/**
 * Unlink a file.
 * \param path file's path
 * \return -1: error, 0: OK
 */
int nbl_nsc_unlink(const char* path);

/**
 * seek offset by whence.
 * \param fd file descriptor
 * \param offset offset
 * \param whence : SEEK_SET/SEEK_CUR/SEEK_END
 * \return seeked offset from begin
 */
off_t nbl_nsc_lseek(int fd, off_t offset, int whence);



#ifdef __cplusplus
}
#endif

#endif  // NSC_NSS_H_
