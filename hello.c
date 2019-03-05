/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26
#define MAX_BLOCK_SIZE 4096		// 4KB block size

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>


// Superblock struct
typedef struct {
    unsigned int magicNumber;   // 0xfa19283e
    unsigned int totalNumBlocks;   // 256 blocks, 256 * 4KB = 1MB
    unsigned int blockSize;     // 4096 BYTES or 4KB 
} Superblock;

// FileSystem struct
typedef struct {
    FILE *FS_FILE;  // Storage
    Superblock sb;  // Superblock
} FileSystem;


// Initialize superblock at start up
static void superblock_init(Superblock *sb, unsigned int totalNumBlocks, unsigned int blockSize) {
    sb->magicNumber = 0xfa19283e;
    sb->totalNumBlocks = totalNumBlocks;
    sb->blockSize = blockSize;
    printf("Initialized superblock with totalNumBlocks = %d and blockSize = %d\n", sb->totalNumBlocks, sb->blockSize);
}

// Initialize file system struct at start up
static void filesys_init(FileSystem *filesystem, FILE *FS_FILE, unsigned int totalNumBlocks, unsigned int blockSize) {
    printf("Initializing file system struct ... \n");
    filesystem->FS_FILE = FS_FILE;
    fseek(FS_FILE, 0, SEEK_SET);
    superblock_init(&filesystem->sb, totalNumBlocks, blockSize);
}

static FileSystem fs;
static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

static int aofs_getattr(const char *path, struct stat *stbuf)
{
	// struct stat is the file's status
	// struct stat {
	// 	dev_t     st_dev;     /* ID of device containing file */
	// 	ino_t     st_ino;     /* inode number */
	// 	mode_t    st_mode;    /* protection */
	// 	nlink_t   st_nlink;   /* number of hard links */
	// 	uid_t     st_uid;     /* user ID of owner */
	// 	gid_t     st_gid;     /* group ID of owner */
	// 	dev_t     st_rdev;    /* device ID (if special file) */
	// 	off_t     st_size;    /* total size, in bytes */
	// 	blksize_t st_blksize; /* blocksize for file system I/O */
	// 	blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
	// 	time_t    st_atime;   /* time of last access */
	// 	time_t    st_mtime;   /* time of last modification */
	// 	time_t    st_ctime;   /* time of last status change */
	// };
	printf("aofs_getattr called\n");
	printf("aofs_getattr: attributes of %s requested\n", path);
	stbuf->st_uid = getuid(); // Owner of the file is user who mounted filesystem
	stbuf->st_gid = getgid(); // Group of file
	stbuf->st_atime = time(NULL); // Last access of file is right now
	stbuf->st_mtime = time(NULL); // Last modification of file is right now
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		// If root directory, return normal file permission 
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} //else if (strcmp(path, hello_path) == 0) {
		// // If /hello, return hello's file permission
		// stbuf->st_mode = S_IFREG | 0444;
		// stbuf->st_nlink = 1;
		// stbuf->st_size = strlen(hello_str);
	//} 
	else {
		// -ENOENT Path doesn't exist
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = 1024;
		// res = -ENOENT;
	}
	return res;
}


// I don't think this will be needed
static int aofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	// -ENOENT Path doesn't exist
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, hello_path + 1, NULL, 0);

	return 0;
}

static int aofs_open(const char *path, struct fuse_file_info *fi)
{
	printf("aofs_open: path = %s\n", path);
	printf("aofs: path = %s\n", path);
	// -ENOENT Path doesn't exist
	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	// -EACCESS Requested permission isn't available
	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int aofs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	printf("aofs_read: path = %s\n", path);
	size_t len;
	(void) fi;
	// -ENOENT Path doesn't exist
	if(strcmp(path, hello_path) != 0)
		return -ENOENT;

	len = strlen(hello_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, hello_str + offset, size);
	} else
		size = 0;

	return size;
}

// NOT WORKING
static int aofs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("create function has been called\n");
	int res;
	res = open(path, fi->flags, mode);
	if(res == -1) {
		return -errno;
	}
	fi->fh = res;
	return 0;
}

// NOT WORKING
static int aofs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	printf("mknod function has been called!\n");
	printf("path = %s\n", path);
	int res;
	if(S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if(res >= 0) {
			res = close(res);
		}
	}
	else if(S_ISFIFO(mode)) {
		res = mkfifo(path, mode);
	}
	else {
		res = mknod(path, mode, rdev);
	}

	if(res == -1) {
		perror("mknod error!\n");
		exit(0);
	}
	
	return 0;
}

static struct fuse_operations aofs_oper = {
	.getattr	= aofs_getattr,
	.readdir	= aofs_readdir,
	.open		= aofs_open,
	.read		= aofs_read,
	.create		= aofs_create,
	.mknod		= aofs_mknod,
};

int main(int argc, char *argv[])
{
	FILE *FS_FILE = fopen("FS_FILE", "r+");
	if(FS_FILE == NULL) {
		FS_FILE = fopen("FS_FILE", "wb+");
		printf("FS_FILE has been created\n");
		filesys_init(&fs, FS_FILE, 256, MAX_BLOCK_SIZE);
	}
	else {
		printf("FS_FILE is not NULL!\n");
	}

	return fuse_main(argc, argv, &aofs_oper, NULL);
}
