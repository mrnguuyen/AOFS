/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26
#define MAX_BLOCK_SIZE 4096			// 4KB block size
#define NUM_BLOCKS 256				// 256 blocks
#define META_RANGE 1096				// 1096 BYTES used for meta data
#define BIT_RANGE 8

// Bitmap operations
// SITED: http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/bit-array.html
// http://www.cs.unh.edu/~jlw/cs610/notes/free-space-mgmt.pdf
#define SETBIT(BitMap,k)     ( BitMap[(k/32)] |= (1 << (k%32)) )
#define CLEARBIT(BitMap,k)   ( BitMap[(k/32)] &= ~(1 << (k%32)) )
#define TESTBIT(BitMap,k)    ( BitMap[(k/32)] & (1 << (k%32)) )


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
	char fileName[24];				// File Name
	unsigned int fileSize;			// File Size
	unsigned int blockIndex;		// File Block Index
	unsigned int nextBlock;			// File Next Block Index if fileSize > 4KB
	mode_t mode;					// File Mode
	time_t timeCreated;				// File Creation Time
	time_t timeUpdated;				// File Updated Time
	time_t timeAccessed;			// File Accessed Time
} Metadata;

// Superblock struct
typedef struct {
	char *magicNumber;
    unsigned int totalNumBlocks;  	// 256 blocks, 256 * 4KB = 1MB
    unsigned int blockSize;     	// 4096 BYTES or 4KB 
	unsigned int BitMap[BIT_RANGE];	// Bitmap of 256 bits to represent blocks of free or occupied
	Metadata metadata[NUM_BLOCKS];	// Meta data goes here of size 256 as well
} Superblock;


// FileSystem struct
typedef struct {
    Superblock sb;  				// Superblock
} FileSystem;


// Initialize superblock at start up
static void superblock_init(Superblock *sb, unsigned int totalNumBlocks, unsigned int blockSize) {
    // sb->magicNumber = 0xfa19283e;
	sb->magicNumber = "0xfa19283e ";
    sb->totalNumBlocks = totalNumBlocks;
    sb->blockSize = blockSize;
	printf("superblock_init: size of int = %lu\n", sizeof(int));
	
	// Initialize all values in bitmap to 0 to set as free blocks
	for(int i = 0; i < totalNumBlocks; i++) {
		sb->metadata[i].fileName[0] = '\0';
		sb->metadata[i].fileSize = 0;
	}

	for(int i = 0; i < BIT_RANGE; i++) {
		sb->BitMap[i] = 0;
	}

	int fd = open("FS_FILE", O_WRONLY);
	if(fd == -1) {
		printf("filesys_init: unable to open FS_FILE\n");
		exit(1);
	}

	lseek(fd, 0, SEEK_SET);
	int res = write(fd, sb->magicNumber, strlen(sb->magicNumber));
	if(res == -1) {
		printf("Unable to write to first block of FS_FILE\n");
		exit(1);
	}

	// Set index 0 of the bitmap = 1 since superblock occupies this
	SETBIT(sb->BitMap, 0);

	for(int i = 0; i < BIT_RANGE; i++) {
		for(unsigned int j = 1 << 31; j > 0; j = j/2) {
			(sb->BitMap[i] & j) ? write(fd, "1", 1) : write(fd, "0", 1);
		}
		write(fd, " ", 1);
	}
	close(fd);
    printf("Initialized superblock with totalNumBlocks = %d and blockSize = %d and created bitmap for free blocks\n", sb->totalNumBlocks, sb->blockSize);
}

// Initialize file system struct at start up
static void filesys_init(FileSystem *filesystem, unsigned int totalNumBlocks, unsigned int blockSize) {
    printf("Initializing file system struct ... \n");
	int fd = open("FS_FILE", O_RDWR | O_TRUNC, 0644); // Open storage file
	if(fd == -1) {
		printf("filesys_init: unable to open FS_FILE\n");
		exit(1);
	}
	printf("filesys_init: totalNumBlocks = %d and blockSize = %d\n", totalNumBlocks, blockSize);
	unsigned int totalBytes = totalNumBlocks * blockSize;
	printf("filesys_init: totalBytes = %d\n", totalBytes);
	int res = ftruncate(fd, totalBytes);
	if(res == -1) {
		printf("filesys_init: unable to truncate file\n");
	}
	printf("filesys_init: truncated FS_FILE with size = %d bytes\n", totalNumBlocks * blockSize);
	int position = lseek(fd, 0, SEEK_SET);
	printf("filesys_init: lseek position = %d\n", position);
	close(fd);
    superblock_init(&filesystem->sb, totalNumBlocks, blockSize);
}

static void filesys_write_bitmap(FileSystem *fs) {

	int fd = open("FS_FILE", O_WRONLY);
	if(fd == -1) {
		printf("filesys_init: unable to open FS_FILE\n");
		exit(1);
	}

	lseek(fd, 11, SEEK_SET);
	SETBIT(fs->sb.BitMap, 0);
	for(int i = 0; i < BIT_RANGE; i++) {
		for(unsigned int j = 1 << 31; j > 0; j = j/2) {
			(fs->sb.BitMap[i] & j) ? write(fd, "1", 1) : write(fd, "0", 1);
		}
		write(fd, " ", 1);
	}
	close(fd);

}

static void filesys_load(FileSystem *fileSystem) {
	// GO THROUGH FS_FILE AND FIND TO SEE IF CONTENT IS THERE. 
	// IF THERE IS, LOAD IT INTO THE BITMAP WITH OCCUPIED = 1
	// IF NOT, SET TO 0
	// FOR EVERY INDEX * 4096 BYTES, CHECK IF CONTENT IS IN THERE
	printf("Loading file system\n");
	// fileSystem->FS_FILE = FS_FILE;
	// fseek(FS_FILE, 0, SEEK_SET);

	// TEMPORARY HERE FOR TESTING
	for(int i = 0; i < NUM_BLOCKS; i++) {
		fileSystem->sb.metadata[i].fileName[0] = '\0';
		fileSystem->sb.metadata[i].fileSize = 0;
	}
}

static int filesys_find_file(FileSystem *fs, char *name) {

	printf("filesys_find_file called\n");
	for(int i = 0; i < NUM_BLOCKS; i++) {
		const char *fileTempName = fs->sb.metadata[i].fileName;
		if(strcmp(fileTempName, name) == 0 && TESTBIT(fs->sb.BitMap, i)) {
			printf("filesys_find_file: File was found with filename =%s\n", fs->sb.metadata[i].fileName);
			return i;
		}
	}
	return -1;
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
	char *name = malloc(strlen(path) + 1);
	strcpy(name, path + 1);
	printf("aofs_getattr: Filename = %s\n", name);
	int res = 0;
	int foundFlag = 0;
	size_t fileSize = 0;
	mode_t mode;
	time_t atime;
	time_t utime;
	int index;

	for(int i = 0; i < 10; i++) {
		// TEST print first 3 fileNames in meta data bit map
		printf("aofs_getattr: Index = %d, fileName = %s\n", i, fs.sb.metadata[i].fileName);
	}

	// Root directory
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		free(name);
		return res;
	} 

	index = filesys_find_file(&fs, name);
	if(index != -1) {
		foundFlag = 1;
		fileSize = fs.sb.metadata[index].fileSize;
		mode = fs.sb.metadata[index].mode;
		atime = fs.sb.metadata[index].timeAccessed;
		utime = fs.sb.metadata[index].timeUpdated;
	}
	// File was found in filesystem
	if(foundFlag == 1) {
		printf("aofs_getattr: %s: foundFlag was set, setting attributes\n", name);
		stbuf->st_mode = mode;
		stbuf->st_nlink = 1;
		stbuf->st_size = fileSize;
		stbuf->st_atime = atime;
		stbuf->st_mtime = utime;
	}
	else {
		printf("aofs_getattr: Path does not exist\n");
		res = -ENOENT;
	}
	free(name);
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

	for(int i = 0; i < NUM_BLOCKS; i++) {
		if(strlen(fs.sb.metadata[i].fileName) != 0) {
			printf("filesys_find_file: File was found with filename = %s\n", fs.sb.metadata[i].fileName);
			filler(buf, fs.sb.metadata[i].fileName, NULL, 0);
		}
	}

	return 0;
}

static int aofs_open(const char *path, struct fuse_file_info *fi)
{
	printf("aofs_open: path = %s\n", path);
	char *name = malloc(strlen(path) + 1);
	strcpy(name, path + 1);	int fd;
	int res;

	// CHECK IF FILE EXISTS
	// By for looping through the file 
	res = filesys_find_file(&fs, name);
	free(name);
	if(res != -1) {
		time_t timeAccessed = time(NULL);
		fs.sb.metadata[res].timeAccessed = timeAccessed;
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
	char *name = malloc(strlen(path) + 1);
	strcpy(name, path + 1);
	int fd;
	int res;
	int index;
	int fileOffSet;
	int position;
	int fileSize;

	fd = open("FS_FILE", O_RDWR, 0644); // Open storage file
	if(fd == -1) {
		printf("aofs_read: FS_FILE was unable to open\n");
		free(name);
		exit(1);
	}
	index = filesys_find_file(&fs, name);
	free(name);
	if(index == -1) {
		printf("filesys_find_file returned -1, unable to find file\n");
		return -1;
	}

	if(fs.sb.metadata[index].nextBlock) {
		time_t timeAccessed = time(NULL);
		printf("aofs_read: Detected a file with a next block filled\n");
		int size1 = 3000;
		int size2 = fs.sb.metadata[index].fileSize - size1;
		fileSize = fs.sb.metadata[index].fileSize;
		char buf1[size1];	// First 3000
		char buf2[size2];	// Leftover
		int nextIndex = fs.sb.metadata[index].nextBlock;

		// Read from first block
		fileOffSet = index * MAX_BLOCK_SIZE;
		fileOffSet = fileOffSet + META_RANGE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		res = read(fd, buf1, size1);
		if(res == -1) {
			printf("aofs_read: Unable to read from FS_FILE first block\n");
			exit(1);
		}

		// Read from second block
		fileOffSet = nextIndex * MAX_BLOCK_SIZE;
		fileOffSet = fileOffSet + META_RANGE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		res = read(fd, buf2, size2);
		if(res == -1) {
			printf("aofs_read: Unable to read from FS_FILe second block\n");
			exit(1);
		}

		// Combine buf1 and buf2
		int j = 0;
		for(int i = 0; i < fileSize; i++) {
			if(i < 3000) {
				// read buf1 into buf
				buf[i] = buf1[i];
			}
			else {
				// read buf2 into buf
				buf[i] = buf2[j];
				j++;
			}
		}
		close(fd);
		fs.sb.metadata[index].timeAccessed = timeAccessed;
		return fileSize;
	}

	time_t timeAccessed = time(NULL);
	
	printf("aofs_read: found file: %s at index = %d\n", fs.sb.metadata[index].fileName, index);
	fileSize = fs.sb.metadata[index].fileSize;
	fileOffSet = index * MAX_BLOCK_SIZE;
	fileOffSet = fileOffSet + META_RANGE;
	position = lseek(fd, fileOffSet, SEEK_SET);
	res = read(fd, buf, fileSize);		// Read first fileSize bytes for content data
	printf("aofs_read: after read ... buf = %s\n", buf);
	close(fd);
	fs.sb.metadata[index].timeAccessed = timeAccessed;
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
	int res1;
	int index;
	int index2;
	int fileOffSet;
	int position;
	int FILE_BIG_FLAG = 0;
	char *name = malloc(strlen(path) + 1);
	strcpy(name, path + 1);
	char metaBuf[META_RANGE] ="";

	// O_TRUNC OVERWRITES EXISTING DATA
	// O_APPEND APPENDS AFTER EXISTING DATA
	// fd = open("FS_FILE", O_RDWR | O_TRUNC, 0644); // Open storage file // O_TRUNC literally overwrites everything
	fd = open("FS_FILE", O_RDWR , 0644); // Open storage file
	if(fd == -1) {
		printf("aofs_write: FS_FILE was unable to open\n");
		free(name);
		exit(1);
	}

	index = filesys_find_file(&fs, name); // Check to make sure file is in FS_FILE
	if(index == -1) {
		printf("filesys_find_file returned -1, unable to find file\n");
		free(name);
		return -1;
	}

	// Check if size of buf is greater than BLOCKSIZE - METARANGE
	if(size > MAX_BLOCK_SIZE - META_RANGE) {
		printf("aofs_write: file's buf is greater than 3000\n");
		FILE_BIG_FLAG = 1;		// set flag to write to 2 files rather than 1
		// Find an empty free block
		for(int i = 1; i < NUM_BLOCKS; i++) {
			if(!TESTBIT(fs.sb.BitMap, i)) {
				printf("Bit %d is free\n", i);
				index2 = i;
				break;
			}
		}
	}
	
	// Size is bigger than 4KB
	if(FILE_BIG_FLAG) {
		time_t timeUpdated = time(NULL);
		time_t timeAccessed = time(NULL);
		printf("aofs_write: size is greater than 3000\n");
		int size2 = size - 3000;		// leftovers after writing to first file
		int firstSize = MAX_BLOCK_SIZE - META_RANGE;
		char buf1[3000];
		char buf2[size2];

		int j = 0;
		for(int i = 0; i < size; i++) {
			if(i >= firstSize) {
				buf2[j] = buf[i];
				j++;
			}
			else {
				buf1[i] = buf[i];
			}
		}


		printf("aofs_write: buf1 = %s\n", buf1);
		printf("aofs_write: buf2 = %s\n", buf2);

		printf("aofs_write: leftover size = %d\n", size2);
		
		// Write to meta data of first block
		fileOffSet = index * MAX_BLOCK_SIZE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		sprintf(metaBuf, "FILE NAME = %s, FILE SIZE = %lu, BLOCK INDEX = %d, MODE = %d, TIME CREATED = %ld, TIME UPDATED = %ld, TIME ACCESSED = %ld", name, strlen(buf), index, fs.sb.metadata[index].mode, fs.sb.metadata[index].timeCreated, timeUpdated, timeAccessed);
		res = write(fd, metaBuf, strlen(metaBuf));
		if(res == -1) {
			printf("aofs_write: File: %s was unable to write to FS_FILE disk with meta data\n", name);
			free(name);
			exit(1);
		}

		// Write to file content of first block
		fileOffSet = fileOffSet + META_RANGE;
		printf("aofs_write: File content fileOffSet = %d\n", fileOffSet);
		position = lseek(fd, fileOffSet, SEEK_SET);
		printf("aofs_write: lseek file content position = %d\n", position);
		res1 = write(fd, &buf1, firstSize);
		if(res1 == -1) {
			printf("aofs_write: File: %s was unable to write to FS_FILE disk with file content data in first block\n", name);
			free(name);
			exit(1);
		}
		printf("aofs_write: this is leftover write buf = %s\n", buf);

		// Write to meta data of second block
		fileOffSet = index2 * MAX_BLOCK_SIZE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		sprintf(metaBuf, "FILE NAME = %s, FILE SIZE = %lu, BLOCK INDEX = %d, MODE = %d, TIME CREATED = %ld, TIME UPDATED = %ld, TIME ACCESSED = %ld", name, strlen(buf), index2, fs.sb.metadata[index].mode, fs.sb.metadata[index].timeCreated, timeUpdated, timeAccessed);
		int res2 = write(fd, metaBuf, strlen(metaBuf));
		if(res2 == -1) {
			printf("aofs_write: File: %s was unable to write to FS_FILE disk with meta data\n", name);
			free(name);
			exit(1);
		}

		// Write to file content of second block
		fileOffSet = fileOffSet + META_RANGE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		int res3 = write(fd, &buf2, strlen(buf2));
		if(res3 == -1) {
			printf("aofs_write: File: %s was unable to write to FS_FILE disk with content data in second block\n", name);
			free(name);
			exit(1);
		}

		SETBIT(fs.sb.BitMap, index2);
		strncpy(fs.sb.metadata[index].fileName, name, sizeof(fs.sb.metadata[index].fileName)-1);
		// strncpy(fs.sb.metadata[index2].fileName, name, sizeof(fs.sb.metadata[index2].fileName)-1);
		fs.sb.metadata[index].fileName[sizeof(fs.sb.metadata[index].fileName)-1] = '\0';
		// fs.sb.metadata[index2].fileName[sizeof(fs.sb.metadata[index2].fileName)-1] = '\0';
		fs.sb.metadata[index].fileSize = size;
		// fs.sb.metadata[index2].fileSize = size;
		fs.sb.metadata[index].blockIndex = index;
		// fs.sb.metadata[index2].blockIndex = index2;
		fs.sb.metadata[index].timeUpdated = timeUpdated;
		// fs.sb.metadata[index2].timeUpdated = timeUpdated;
		fs.sb.metadata[index].timeAccessed = timeAccessed;
		// fs.sb.metadata[index2].timeAccessed = timeAccessed;
		fs.sb.metadata[index].nextBlock = index2;
		printf("aofs_write: time updated = %ld\n", fs.sb.metadata[index].timeUpdated);
		printf("aofs_write: metadata fileSize = %d\n", fs.sb.metadata[index].fileSize);

		int totalBytes = NUM_BLOCKS * MAX_BLOCK_SIZE;
		int trunc = ftruncate(fd, totalBytes);
		if(trunc == -1) {
			printf("aofs_write: unable to truncate file\n");
		}
		filesys_write_bitmap(&fs);

		close(fd);
		free(name);
		return size;
	}


	printf("aofs_write: found file: %s at index = %d\n", fs.sb.metadata[index].fileName, index);
	time_t timeUpdated = time(NULL);
	time_t timeAccessed = time(NULL);

	// Write to block's metadata
	fileOffSet = index * MAX_BLOCK_SIZE;
	printf("aofs_write: metadata fileOffSet = %d\n", fileOffSet);
	position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_write: lseek metadata position = %d\n", position);
	sprintf(metaBuf, "FILE NAME = %s, FILE SIZE = %lu, BLOCK INDEX = %d, MODE = %d, TIME CREATED = %ld, TIME UPDATED = %ld, TIME ACCESSED = %ld", name, strlen(buf), index, fs.sb.metadata[index].mode, fs.sb.metadata[index].timeCreated, timeUpdated, timeAccessed);
	printf("aofs_write: metaBuf = %s\n", metaBuf);
	res = write(fd, metaBuf, strlen(metaBuf));
	if(res == -1) {
		printf("aofs_write: File: %s was unable to write to FS_FILE disk with meta data\n", name);
		free(name);
		exit(1);
	}

	// Write to block's content data
	fileOffSet = fileOffSet + META_RANGE;
	printf("aofs_write: File content fileOffSet = %d\n", fileOffSet);
	position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_write: lseek file content position = %d\n", position);
	res1 = write(fd, buf, size);
	if(res1 == -1) {
		printf("aofs_write: File: %s was unable to write to FS_FILE disk with file content data\n", name);
		free(name);
		exit(1);
	}

	strncpy(fs.sb.metadata[index].fileName, name, sizeof(fs.sb.metadata[index].fileName)-1);
	fs.sb.metadata[index].fileName[sizeof(fs.sb.metadata[index].fileName)-1] = '\0';
	fs.sb.metadata[index].fileSize = size;
	fs.sb.metadata[index].blockIndex = index;
	// fs.sb.metadata[index].mode = S_IFREG | 0644;
	fs.sb.metadata[index].timeUpdated = timeUpdated;
	fs.sb.metadata[index].timeAccessed = timeAccessed;
	printf("aofs_write: time updated = %ld\n", fs.sb.metadata[index].timeUpdated);
	printf("aofs_write: metadata fileSize = %d\n", fs.sb.metadata[index].fileSize);

	int totalBytes = NUM_BLOCKS * MAX_BLOCK_SIZE;
	int trunc = ftruncate(fd, totalBytes);
	if(trunc == -1) {
		printf("aofs_write: unable to truncate file\n");
	}

	close(fd);
	filesys_write_bitmap(&fs);
	free(name);
	return size;
}

static int aofs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{

	printf("aofs_create: path = %s\n", path);
	char *name = malloc(strlen(path) + 1);
	strcpy(name, path + 1);
	printf("aofs_create: filename = %s\n", name);
	
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
	char buf[META_RANGE] = "";
	char *emptyBuf = "";
	fd = open("FS_FILE", O_WRONLY);
	if(fd == -1) {
		printf("aofs_create: FS_FILE did not open correctly\n");
		free(name);
		exit(1);
	}

	// Find free block inside of superblock
	// After writing to FS_FILE, set bitmap[index] = 1 for occupied
	for(int i = 1; i < NUM_BLOCKS; i++) {
		if(!TESTBIT(fs.sb.BitMap, i)) {
			printf("Bit %d is free\n", i);
			index = i;
			break;
		}
	}

	time_t timeCreated = time(NULL);
	time_t timeAccessed = time(NULL);

	// Write to block's metadata
	printf("aofs_create: Free index found at index = %d\n", index);
	fileOffSet = index * MAX_BLOCK_SIZE;
	printf("aofs_create: File meta data Offset = %d\n", fileOffSet);	
	int position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_create: lseek meta data position = %d\n", position);
	sprintf(buf, "FILE NAME = %s, FILE SIZE = %d, BLOCK INDEX = %d, MODE = %d, TIME CREATED = %ld, TIME UPDATED = 0, TIME ACCESSED = %ld", name, 0, index, mode, timeCreated, timeAccessed);
	printf("aofs_create: buf = %s\n", buf);
	res = write(fd, &buf, strlen(buf));
	if(res == -1) {
		printf("aofs_create: File: %s was unable to write to FS_FILE disk Meta Data \n", name);
		free(name);
		exit(1);
	}

	// Write to block's content data
	fileOffSet = fileOffSet + META_RANGE;
	printf("aofs_create: File content data Offset = %d\n", fileOffSet);
	position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_create: lseek file content position = %d\n", position);
	res = write(fd, emptyBuf, 0);
	if(res == -1) {
		printf("aofs_create: File: %s was unable to write to FS_FILE disk Content Data \n", name);
		free(name);
		exit(1);
	}
	SETBIT(fs.sb.BitMap, index);
	strncpy(fs.sb.metadata[index].fileName, name, sizeof(fs.sb.metadata[index].fileName)-1);
	fs.sb.metadata[index].fileName[sizeof(fs.sb.metadata[index].fileName)-1] = '\0';
	fs.sb.metadata[index].fileSize = 0;
	fs.sb.metadata[index].blockIndex = index;
	fs.sb.metadata[index].mode = mode;
	fs.sb.metadata[index].timeCreated = timeCreated;
	fs.sb.metadata[index].timeAccessed = timeAccessed;	
	printf("aofs_create: FS_FILE time created = %ld\n", fs.sb.metadata[index].timeCreated);
	printf("aofs_create: FS_FILE file name at index %d = %s\n", index, fs.sb.metadata[index].fileName);
	
	int totalBytes = NUM_BLOCKS * MAX_BLOCK_SIZE;
	int trunc = ftruncate(fd, totalBytes);
	if(trunc == -1) {
		printf("aofs_write: unable to truncate file\n");
	}

	close(fd);
	filesys_write_bitmap(&fs);
	free(name);
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


static int aofs_unlink(const char *path) {
	printf("aofs_unlink function called\n");
	char *name = malloc(strlen(path) + 1);
	strcpy(name, path + 1);
	printf("aofs_unlink: filename = %s\n", name);

	int fileSize;

	// find the file name in the file system
	int index = filesys_find_file(&fs, name);
	if(index == -1) {
		printf("filesys_find_file returned -1, unable to find file\n");
		free(name);
		return -1;
	}

	fileSize = fs.sb.metadata[index].fileSize;
	char emptyBuf[fileSize];
	for(int i = 0; i < fileSize; i++) {
		emptyBuf[i] = 0;
	}
	
	char metaBuf[META_RANGE];
	for(int i = 0; i < META_RANGE; i++) {
		metaBuf[i] = 0;
	}
	
	// index has file we want to delete from file system
	// open up FS_FILE
	int fd = open("FS_FILE", O_WRONLY); // Open storage file
	if(fd == -1) {
		printf("aofs_write: FS_FILE was unable to open\n");
		free(name);
		exit(1);
	}

	// Check if there is nextBlock
	int nextIndex = fs.sb.metadata[index].nextBlock;
	if(nextIndex) {
		printf("aofs_unlink: detected next block!\n");
		char emptyThreeBuf[3000];
		for(int i = 0; i < 3000; i++) {
			emptyThreeBuf[i] = 0;
		}

		// Write to first block meta data
		int fileOffSet = index * MAX_BLOCK_SIZE;
		int position = lseek(fd, fileOffSet, SEEK_SET);
		int res = write(fd, &metaBuf, META_RANGE);
		if(res == -1) {
			printf("aofs_unlink: Unable to write to FS_FILE disk first block meta data\n");
			free(name);
			exit(1);
		}

		// Write to frst block file content data
		fileOffSet = fileOffSet + META_RANGE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		res = write(fd, &emptyThreeBuf, 3000);
		if(res == -1) {
			printf("aofs_unlink: Unable to write to FS_FILE disk first block content data\n");
			free(name);
			exit(1);
		}

		// Write to second block meta data
		fileOffSet = nextIndex * MAX_BLOCK_SIZE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		res = write(fd, &metaBuf, META_RANGE);
		if(res == -1) {
			printf("aofs_unlink: Unable to write to FS_FILE disk second block meta data\n");
			free(name);
			exit(1);
		}

		// Write to second block content data
		fileOffSet = fileOffSet + META_RANGE;
		position = lseek(fd, fileOffSet, SEEK_SET);
		res = write(fd, &emptyThreeBuf, 3000);
		if(res == -1) {
			printf("aofs_unlink: Unable to write to FS_FILE disk second block meta data\n");
			free(name);
			exit(1);
		}
		// Upon successful deletion of the file
		CLEARBIT(fs.sb.BitMap, index);
		CLEARBIT(fs.sb.BitMap, nextIndex);
		for(int i = 0; i < 24; i++) {
			fs.sb.metadata[index].fileName[i] = 0;
		}
		fs.sb.metadata[index].fileSize = 0;
		fs.sb.metadata[index].blockIndex = 0;
		fs.sb.metadata[index].mode = 0;
		fs.sb.metadata[index].timeCreated = 0;
		fs.sb.metadata[index].timeUpdated = 0;

		int totalBytes = NUM_BLOCKS * MAX_BLOCK_SIZE;
		int trunc = ftruncate(fd, totalBytes);
		if(trunc == -1) {
			printf("aofs_write: unable to truncate file\n");
		}
		close(fd);
		filesys_write_bitmap(&fs);
		return 0;

	}

	// Write to meta data
	int fileOffSet = index * MAX_BLOCK_SIZE;		// Meta data offset
	printf("aofs_unlink: File meta data Offset = %d\n", fileOffSet);	
	int position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_unlink: lseek meta data position = %d\n", position);
	int res = write(fd, &metaBuf, META_RANGE);
	if(res == -1) {
		printf("aofs_unlink: File: %s was unable to write to FS_FILE disk Meta Data \n", name);
		free(name);
		exit(1);
	}

	// Write to block's content data
	fileOffSet = fileOffSet + META_RANGE;
	printf("aofs_unlink: File content data Offset = %d\n", fileOffSet);
	position = lseek(fd, fileOffSet, SEEK_SET);
	printf("aofs_unlink: lseek file content position = %d\n", position);
	res = write(fd, &emptyBuf, fileSize);
	if(res == -1) {
		printf("aofs_unlink: File: %s was unable to write to FS_FILE disk Content Data \n", name);
		free(name);
		exit(1);
	}

	// Upon successful deletion of the file
	CLEARBIT(fs.sb.BitMap, index);
	for(int i = 0; i < 24; i++) {
		fs.sb.metadata[index].fileName[i] = 0;
	}
	fs.sb.metadata[index].fileSize = 0;
	fs.sb.metadata[index].blockIndex = 0;
	fs.sb.metadata[index].mode = 0;
	fs.sb.metadata[index].timeCreated = 0;
	fs.sb.metadata[index].timeUpdated = 0;

	int totalBytes = NUM_BLOCKS * MAX_BLOCK_SIZE;
	int trunc = ftruncate(fd, totalBytes);
	if(trunc == -1) {
		printf("aofs_write: unable to truncate file\n");
	}
	close(fd);
	filesys_write_bitmap(&fs);
	return 0;
}

static int aofs_statfs(const char *path, struct statvfs *stbuf) {
	printf("aofs_statfs function called\n");
	int res;
	char *name = malloc(strlen(path) + 1);
	strcpy(name, path + 1);
	res = statvfs(name, stbuf);
	free(name);
	if(res == -ENOENT) {
		return -errno;
	}
	return 0;
}

static int aofs_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;
	printf("aofs_truncate function called\n");

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
	.unlink		= aofs_unlink,
	.statfs		= aofs_statfs,
	.truncate	= aofs_truncate,
};

int main(int argc, char *argv[])
{
	FILE *FS_FILE = fopen("FS_FILE", "r+");
	if(FS_FILE == NULL) {
		FS_FILE = fopen("FS_FILE", "wb+");		// For write and read
		printf("FS_FILE has been created\n");
		fclose(FS_FILE);
		filesys_init(&fs, NUM_BLOCKS, MAX_BLOCK_SIZE);
	}
	else {
		printf("FS_FILE is not NULL!\n");
		filesys_load(&fs);
		fclose(FS_FILE);
	}

	return fuse_main(argc, argv, &aofs_oper, NULL);
}
