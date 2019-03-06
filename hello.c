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
    unsigned int magicNumber;   	// 0xfa19283e
    unsigned int totalNumBlocks;  	// 256 blocks, 256 * 4KB = 1MB
    unsigned int blockSize;     	// 4096 BYTES or 4KB 
	int bitmap[256];				// Bitmap of 256 bits
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
	
	// Initialize all values in bitmap to 0 to set as free bits/blocks
	for(int i = 0; i < totalNumBlocks; i++) {
		sb->bitmap[i] = 0;
	}

    printf("Initialized superblock with totalNumBlocks = %d and blockSize = %d and created bitmap for free blocks\n", sb->totalNumBlocks, sb->blockSize);
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
static const char *hi_path = "/hi";

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
	printf("aofs_getattr: attributes of %s requested\n", path);
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} //else if (strcmp(path, hi_path) == 0) {
	// 	printf("aofs_getattr: retrieving attributes of hi file\n");
	// 	stbuf->st_mode = S_IFREG | 0444;
	// 	stbuf->st_nlink = 1;
	// } 
	else {
		printf("aofs_getattr: Path does not exist\n");
		res = -ENOENT;
	}
	return res;
}


static int aofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	printf("aofs_readdir: Grabbing list of files \n");

	// -ENOENT Path doesn't exist
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0); 		// Current directory
	filler(buf, "..", NULL, 0); 	// Parent directory
	filler(buf, hello_path + 1, NULL, 0);	// Filler for hello file
	// filler(buf, hi_path + 1, NULL, 0);		// This fills inside newHelloFS with filler file with /hi unless we add +1

	return 0;
}

static int aofs_open(const char *path, struct fuse_file_info *fi)
{
	printf("aofs_open: path = %s\n", path);
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

// Update the last access time of the given object from ts[0] and the 
// last modification time from ts[1]. Both time specifications are given 
// to nanosecond resolution, but your filesystem doesn't have to be that 
// precise; see utimensat(2) for full details. Note that the time specifications 
// are allowed to have certain special values; however, I don't know if FUSE functions 
// have to support them. This function isn't necessary but is nice to have in a fully functional filesystem.
static int aofs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi)
{
	(void) fi;
	int res;
	
	printf("aofs_utimen: path = %s\n", path);
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
			return -errno;
	return 0;
}

static int aofs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{

	printf("aofs_create: path = %s\n", path);
	char *fileName = path + 1;
	printf("aofs_create: filename = %s\n", fileName);
	
	int res;
	res = open(fileName, O_WRONLY|O_CREAT|O_TRUNC, mode);
	if (res == -1)
		printf("File: %s was unable to be created\n", fileName);
	fi->fh = res;
	printf("aofs_create: %s was created\n", fileName);
	return 0;
}

// NOT WORKING
static int aofs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	printf("mknod function has been called!\n");
	printf("path = %s\n", path);
	// int res;
	// if(S_ISREG(mode)) {
	// 	res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
	// 	if(res >= 0) {
	// 		res = close(res);
	// 	}
	// }
	// else if(S_ISFIFO(mode)) {
	// 	res = mkfifo(path, mode);
	// }
	// else {
	// 	res = mknod(path, mode, rdev);
	// }

	// if(res == -1) {
	// 	perror("mknod error!\n");
	// 	exit(0);
	// }
	
	return 0;
}

static struct fuse_operations aofs_oper = {
	.getattr	= aofs_getattr,
	.readdir	= aofs_readdir,
	.open		= aofs_open,
	.read		= aofs_read,
	.utimens	= aofs_utimens,
	.mknod		= aofs_mknod,
	.create		= aofs_create,
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
