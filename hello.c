/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26
#define MAX_BLOCK_SIZE 4096		// 4KB block size
#define NUM_BLOCKS 256
#define META_RANGE 96

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

// Metadata struct
typedef struct {
	char *fileName;
	unsigned int fileSize;
	unsigned int blockIndex;
	mode_t mode;
} Metadata;

// Superblock struct
typedef struct {
    unsigned int magicNumber;   	// 0xfa19283e
    unsigned int totalNumBlocks;  	// 256 blocks, 256 * 4KB = 1MB
    unsigned int blockSize;     	// 4096 BYTES or 4KB 
	char bitmap[NUM_BLOCKS];		// Bitmap of 256 bits to represent blocks of free or occupied
	Metadata metadata[NUM_BLOCKS];	// Meta data goes here of size 256 as well
} Superblock;


// FileSystem struct
typedef struct {
    FILE *FS_FILE;  	// Storage
    Superblock sb;  	// Superblock
} FileSystem;


// Initialize superblock at start up
static void superblock_init(Superblock *sb, unsigned int totalNumBlocks, unsigned int blockSize) {
    sb->magicNumber = 0xfa19283e;
    sb->totalNumBlocks = totalNumBlocks;
    sb->blockSize = blockSize;
	
	// Initialize all values in bitmap to 0 to set as free blocks
	for(int i = 0; i < totalNumBlocks; i++) {
		sb->bitmap[i] = 0;
		sb->metadata[i].fileName = "";
		sb->metadata[i].fileSize = 0;
	}
    printf("Initialized superblock with totalNumBlocks = %d and blockSize = %d and created bitmap for free blocks\n", sb->totalNumBlocks, sb->blockSize);
}

// Initialize file system struct at start up
static void filesys_init(FileSystem *filesystem, FILE *FS_FILE, unsigned int totalNumBlocks, unsigned int blockSize) {
    printf("Initializing file system struct ... \n");
	// truncate file to specific size
    filesystem->FS_FILE = FS_FILE;		// Initialize FS_FILE to FileSystem
    fseek(FS_FILE, 0, SEEK_SET);		// Start iterator at beginning of FS_FILE
    superblock_init(&filesystem->sb, totalNumBlocks, blockSize);
}

static void filesys_load(FileSystem *fileSystem, FILE *FS_FILE) {
	// GO THROUGH FS_FILE AND FIND TO SEE IF CONTENT IS THERE. 
	// IF THERE IS, LOAD IT INTO THE BITMAP WITH OCCUPIED = 1
	// IF NOT, SET TO 0
	// FOR EVERY INDEX * 4096 BYTES, CHECK IF CONTENT IS IN THERE
	printf("Loading file system\n");
	fileSystem->FS_FILE = FS_FILE;
	fseek(FS_FILE, 0, SEEK_SET);

	// TEMPORARY HERE FOR TESTING
	for(int i = 0; i < NUM_BLOCKS; i++) {
		fileSystem->sb.bitmap[i] = 0;
		fileSystem->sb.metadata[i].fileName = "";
		fileSystem->sb.metadata[i].fileSize = 0;
	}
}

static void filesys_bitmap_set(FileSystem *fs, char *fileName, int index, size_t size) {
	printf("filesys_bitmap_set: filename = %s and index = %d\n", fileName, index);
	fs->sb.bitmap[index] = 1;
	fs->sb.metadata[index].fileName = fileName;
	fs->sb.metadata[index].fileSize = size;
}

static int filesys_find_file(FileSystem *fs, char *fileName) {
	printf("filesys_find_file called\n");
	for(int i = 0; i < NUM_BLOCKS; i++) {
		char *fileTempName = fs->sb.metadata[i].fileName;
		int bitValueAtIndex = fs->sb.bitmap[i];
		if(strcmp(fileTempName, fileName) == 0 && bitValueAtIndex == 1) {
			printf("filesys_find_file: File was found with filename = %s\n", fs->sb.metadata[i].fileName);
			return i;
		}
	}
	return -1; // File was not found in file system
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
	printf("aofs_getattr: attributes of path = %s requested\n", path);
	memset(stbuf, 0, sizeof(struct stat));
	int res = 0;
	int foundFlag = 0;
	size_t fileSize = 0;
	char *fileName = path + 1;

	// Root directory
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return res;
	} 
	res = filesys_find_file(&fs, fileName);
	if(res != -1) {
		foundFlag = 1;
		fileSize = fs.sb.metadata[res].fileSize;
	}
	// hello example path
	if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;			// read only
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	// File was found in filesystem
	} else if(foundFlag == 1) {
		printf("aofs_getattr: %s: foundFlag was set, setting attributes\n", fileName);
		stbuf->st_mode = S_IFREG | 0644;			// read & write
		stbuf->st_nlink = 1;
		stbuf->st_size = fileSize;
	}
	else {
		printf("aofs_getattr: Path does not exist\n");
		res = -ENOENT;
	}
	return res;
}

// Not needed
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

	// printf("aofs_readdir: before filler for fileName\n");
	// for(int i = 0; i < NUM_BLOCKS; i++) {
	// 	if(fs.sb.metadata[i].fileName != NULL && fs.sb.bitmap[i] == 1) {
	// 		printf("aofs_readdir: File was found with filename = %s\n", fs.sb.metadata[i].fileName);
	// 		filler(buf, fs.sb.metadata[i].fileName, NULL, 0);
	// 	}
	// }


	// filesys_find_files_readdir(&fs, buf, filler);

	return 0;
}

static int aofs_open(const char *path, struct fuse_file_info *fi)
{
	printf("aofs_open: path = %s\n", path);
	char *fileName = path + 1;
	int fd;
	int res;

	// CHECK IF FILE EXISTS
	// By for looping through the file 
	res = filesys_find_file(&fs, fileName);
	if(res != -1) {
		return 0;
	}
	// -EACCESS Requested permission isn't available
	else if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;
	else {
		return -ENOENT;
	}
}

static int aofs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	printf("aofs_read: path = %s\n", path);
	printf("aofs_read: buf = %s\n", buf);
	size_t len;
	(void) fi;
	char *fileName = path + 1;
	int fd;
	int res;
	int index;
	int fileOffSet;
	int position;
	int fileSize;

	fd = open("FS_FILE", O_RDWR, 0644); // Open storage file
	if(fd == -1) {
		printf("aofs_read: FS_FILE was unable to open\n");
		exit(1);
	}
	index = filesys_find_file(&fs, fileName);
	if(index == -1) {
		printf("filesys_find_file returned -1, unable to find file\n");
		return -1;
	}
	fileSize = fs.sb.metadata[index].fileSize;
	fileOffSet = index * MAX_BLOCK_SIZE;
	fileOffSet = fileOffSet + META_RANGE;
	position = lseek(fd, fileOffSet, SEEK_SET);
	res = read(fd, buf, fileSize);		// Read first fileSize bytes for content data
	printf("aofs_read: after read ... buf = %s\n", buf);

	// ERRORS RETURNED
	// fuse: read too many bytes
	// fuse: writing device: Invalid argument


	// -ENOENT Path doesn't exist
	// if(strcmp(path, hello_path) != 0)
	// 	return -ENOENT;
	// len = strlen(hello_str);
	// if (offset < len) {
	// 	if (offset + size > len)
	// 		size = len - offset;
	// 	memcpy(buf, hello_str + offset, size);
	// } else
	// 	size = 0;

	return res;
}

static int aofs_write(const char *path, const char *buf, size_t size, off_t offset, 
				struct fuse_file_info *fi)
{
	printf("aofs_write: path = %s\n", path);
	printf("aofs_write: buf = %s\n", buf);
	printf("aofs_write: size = %zu\n", size);
	printf("aofs_write: offset = %ld\n", offset);
	int fd;
	int res;
	int index;
	int fileOffSet;
	int position;
	char *fileName = path + 1;
	char metaBuf[96] ="";

	// fd = open("FS_FILE", O_WRONLY | O_TRUNC, 0644); // Open storage file
	fd = open("FS_FILE", O_RDWR, 0644); // Open storage file
	if(fd == -1) {
		printf("aofs_write: FS_FILE was unable to open\n");
		exit(1);
	}

	index = filesys_find_file(&fs, fileName); // Check to make sure file is in FS_FILE
	if(index == -1) {
		printf("filesys_find_file returned -1, unable to find file\n");
		return -1;
	}

	// Write to block's metadata
	fileOffSet = index * MAX_BLOCK_SIZE;
	printf("aofs_write: metadata fileOffSet = %d\n", fileOffSet);
	position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_write: lseek metadata position = %d\n", position);
	sprintf(metaBuf, "fileName = %s, fileSize = %lu, blockIndex = %d", fileName, strlen(buf), index);
	printf("aofs_write: metaBuf = %s\n", metaBuf);
	res = write(fd, &metaBuf, strlen(metaBuf));
	if(res == -1) {
		printf("aofs_write: File: %s was unable to write to FS_FILE disk with meta data\n", fileName);
		exit(1);
	}

	// Write to block's content data
	fileOffSet = fileOffSet + META_RANGE;
	printf("aofs_write: File content fileOffSet = %d\n", fileOffSet);
	position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_write: lseek file content position = %d\n", position);
	res = write(fd, buf, size);
	if(res == -1) {
		printf("aofs_write: File: %s was unable to write to FS_FILE disk with file content data\n", fileName);
		exit(1);
	}

	fs.sb.metadata[index].fileName = fileName;
	fs.sb.metadata[index].fileSize = size;
	fs.sb.metadata[index].blockIndex = index;
	printf("aofs_write: metadata fileSize = %d\n", fs.sb.metadata[index].fileSize);
	close(fd);
	return 0;
}

static int aofs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{

	printf("aofs_create: path = %s\n", path);
	char *fileName = path + 1;
	printf("aofs_create: filename = %s\n", fileName);
	
	/*
		When you create a file, you open FS_FILE and write to FS_FILE of the file content
		by finding the first free block. In order to find a free block, iterate through the bitmap
		and once you find a free block, do index of where bitmap was found multiplied by 4096 BYTES
		which becomes the index byte to write to the FS_FILE. 
	*/

	int res;
	int fd;
	int index;
	int fileOffSet;
	char buf[95] = "";
	char *emptyBuf = "";
	fd = open("FS_FILE", O_RDWR | O_TRUNC, mode);
	if(fd == -1) {
		printf("aofs_create: FS_FILE did not open correctly\n");
		exit(1);
	}
	// Find free block inside of superblock
	// After writing to FS_FILE, set bitmap[index] = 1 for occupied
	for(int i = 0; i < NUM_BLOCKS; i++) {
		if(fs.sb.bitmap[i] == 0) {
			// This is a free block
			index = i;
			break;
		}
	}	

	// Write to block's metadata
	printf("aofs_create: Free index found at index = %d\n", index);
	fileOffSet = index * MAX_BLOCK_SIZE;
	printf("aofs_create: File meta data Offset = %d\n", fileOffSet);	
	int position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_create: lseek meta data position = %d\n", position);
	sprintf(buf, "fileName = %s, fileSize = %d, blockIndex = %d", fileName, 0, index);
	printf("aofs_create: buf = %s\n", buf);
	res = write(fd, &buf, strlen(buf));
	if(res == -1) {
		printf("aofs_create: File: %s was unable to write to FS_FILE disk Meta Data \n", fileName);
		exit(1);
	}

	// Write to block's content data
	fileOffSet = fileOffSet + META_RANGE;
	printf("aofs_create: File content data Offset = %d\n", fileOffSet);
	position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_create: lseek file content position = %d\n", position);
	res = write(fd, emptyBuf, 0);
	if(res == -1) {
		printf("aofs_create: File: %s was unable to write to FS_FILE disk Content Data \n", fileName);
		exit(1);
	}
	fs.sb.bitmap[index] = 1;
	fs.sb.metadata[index].fileName = fileName;
	fs.sb.metadata[index].fileSize = 0;
	fs.sb.metadata[index].blockIndex = index;
	// filesys_bitmap_set(&fs, fileName, i, 0);
	printf("aofs_create: FS_FILE file name at index %d = %s\n", index, fs.sb.metadata[index].fileName);
	close(fd);
	return 0;
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


// NOT WORKING
static int aofs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	printf("mknod function has been called!\n");
	printf("path = %s\n", path);
	return 0;
}

static int aofs_access(const char *path, int i)
{
	printf("aofs_access called\n");
    return 0;
}

static struct fuse_operations aofs_oper = {
	.getattr	= aofs_getattr,
	.readdir	= aofs_readdir,
	.open		= aofs_open,
	.read		= aofs_read,
	.create		= aofs_create,
	.write		= aofs_write,
	.mknod		= aofs_mknod,
	.access		= aofs_access,
	.utimens	= aofs_utimens,
};

int main(int argc, char *argv[])
{
	FILE *FS_FILE = fopen("FS_FILE", "r+");
	if(FS_FILE == NULL) {
		FS_FILE = fopen("FS_FILE", "wb+");		// For write and read
		printf("FS_FILE has been created\n");
		filesys_init(&fs, FS_FILE, NUM_BLOCKS, MAX_BLOCK_SIZE);
	}
	else {
		printf("FS_FILE is not NULL!\n");
		filesys_load(&fs, FS_FILE);
	}

	return fuse_main(argc, argv, &aofs_oper, NULL);
}
