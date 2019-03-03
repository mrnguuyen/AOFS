#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

typedef struct {
    unsigned int magicNumber;
    // Bitmap goes here
} Superblock;