/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//Total length of a file name with ".extension"
#define FILE_NAME_LENGTH_WITH_EXTENSION (MAX_FILENAME + MAX_EXTENSION + 2)

//size of a disk block
#define	BLOCK_SIZE 512

// Number of blocks in this disk. Set to 5MB worth of blocks
// 5 MB disk -> 2^20 * 5 = 5242880 bytes 
// Number of blocks needed for the bitmap
// 10240/512 = 20 blocks allocated for the bitmap
#define NUM_BLOCKS_ON_DISK 10240

#define NUM_BLOCKS_FOR_BITMAP (NUM_BLOCKS_ON_DISK/BLOCK_SIZE)
/**
 *  Bitmap will contain 20 * BLOCK_SIZE bytes.
 */
struct cs1550_bitmap
{
    unsigned char bitmap[NUM_BLOCKS_ON_DISK];
};

typedef struct cs1550_bitmap cs1550_bitmap;
//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct cs1550_disk_block
{
	//The next disk block, if needed. This is the next pointer in the linked 
	//allocation list
	long nNextBlock;

	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

/**
 *  Allocate the dname, fname, or fext contains junks.
 *  If directory, file, or extension is not specified, it might cause problem.
 */
static void allocSpace(const char* path, char* dname, char* fname, char* fext)
{
    memset(dname, 0, MAX_FILENAME + 1);
    memset(fname, 0, MAX_FILENAME + 1);
    memset(fext, 0, MAX_EXTENSION + 1);
    sscanf(path, "/%[^/]/%[^.].%s", dname, fname, fext);
}


/**
 *  Return offset for directory in .disk file
 *  The root directory occupies the first block of the .disk file
 *  Return -1 if directory is not found
 */
static int getDirectoryOffset(FILE *fp, char *dname)
{
	cs1550_root_directory rootDirectory;

	//seek to the beginning of the file
	fseek(fp, 0, SEEK_SET);

	if(fread(&rootDirectory, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
	{
		int numOfDirectories = rootDirectory.nDirectories;
		//printf("Get directory offset: There are %d directories on disk.\n", numOfDirectories);

		int index;
		for(index = 0 ;index < numOfDirectories;index++)
		{
			if(strcmp(dname, rootDirectory.directories[index].dname) == 0)
			{
				return rootDirectory.directories[index].nStartBlock;
			}
		}
	}

	return -1;
}

/**
 *  Update directory after a file is being written in .disk file
 *  Return -1 if there is an error
 */
static int updateDirectory(FILE *fp, cs1550_directory_entry *directory, int position)
{
	int returnValue = -1;

	//seek to the beginning of the file
	fseek(fp, sizeof(cs1550_directory_entry) * position, SEEK_SET);

	int isWriteSuccess = fwrite(directory, sizeof(cs1550_directory_entry), 1, fp);
	if(isWriteSuccess == 1)
	{
		printf("Directory updated.\n");
		returnValue = 0;
	}
	return returnValue;
}

/**
 *  Return index of a file under a directory on .disk file
 *  The root directory occupies the first block of the .disk file
 *  Return -1 if the file with the given name is not found on .disk
 */
static int findFileIndexUnderDirectory(cs1550_directory_entry *subDirectory, char *fname, char *fext)
{
	int index;
	for(index = 0;index < subDirectory->nFiles;index++)
	{
		if(strcmp((subDirectory->files[index]).fname, fname) == 0 
			&& strcmp((subDirectory->files[index]).fext, fext) == 0)
		{
			return index;
		}
	}
	return -1;
}

/**
 *  load a block into memory from .disk 
 *  The root directory occupies the first block of the .disk file
 *  Return -1 if the disk block is not found on .disk
 */
static int readBlockToMem(FILE *fp, cs1550_disk_block *destBlock, int position)
{
	int returnValue = -1;
	if(fseek(fp, sizeof(cs1550_disk_block) * position, SEEK_SET) == 0)
	{
		if(fread(destBlock, sizeof(cs1550_disk_block), 1, fp) == 1)
		{
			returnValue = 0;
		}
	}
	return returnValue;
}

/**
 *  write a block into .disk from memory
 *  The root directory occupies the first block of the .disk file
 *  Return -1 if the disk block is not in main memory
 */
static int writeBlockToDisk(FILE *fp, cs1550_disk_block *destBlock, int position)
{
	int returnValue = -1;
	if(fseek(fp, sizeof(cs1550_disk_block) * position, SEEK_SET) == 0)
	{
		if(fwrite(destBlock, sizeof(cs1550_disk_block), 1, fp) == 1)
		{
			returnValue = 0;
		}
	}
	return returnValue;
}
/**
 *  Transfer data from temp block to buffer.
 *  Return data remaining for transfer
 */
static int transferBlockToBuffer(cs1550_disk_block *block, char *buf, int position, int dataRemaining)
{
	while (dataRemaining > 0)
	{
		//requested block is below position for reading
		if (position > MAX_DATA_IN_BLOCK)
		{
			return dataRemaining;
		}
		else
		{
			*buf = block->data[position];
			buf++;
			dataRemaining--;
			position++;
		}
	}
	return dataRemaining;
}

/**
 *  Transfer data from buffer to block
 *  Return data remaining for transfer
 */
 static int transferBufferToBlock(cs1550_disk_block *block, const char *buf, int position, int dataRemaining)
 {
 	while (dataRemaining > 0)
	{
		//this block is not enough
		if (position > MAX_DATA_IN_BLOCK)
		{
			return dataRemaining;
		}
		else
		{
			block->data[position] = *buf;
			buf--;
			dataRemaining--;
			position++;
		}
	}
	return dataRemaining;
 }

/**
 *  Find a free block based on bitmap
 */
static int findFreeBlock(FILE* fp)
{
    int ret = -1;
    
    int index;

    struct cs1550_bitmap myBitmap;

    //seek to the beginning of the bitmap
    fseek(fp, -sizeof(cs1550_bitmap), SEEK_END);

    fread(&myBitmap, 1, sizeof(cs1550_bitmap), fp);

    //index 0 -> fisrt block, is used to store the root directory
    //index NUM_BLOCKS_ON_DISK - 20 -> last 20 blocks, reserved for bitmap
    //index NUM_BLOCKS_ON_DISK - 21 -> to store the startblock pointer for write and mknow operation
    myBitmap.bitmap[0] = 1;
    myBitmap.bitmap[NUM_BLOCKS_ON_DISK - 21] = 1;

    for(index = NUM_BLOCKS_ON_DISK - 20;index < NUM_BLOCKS_ON_DISK;index++)
    {
    	myBitmap.bitmap[index] = 1;
    }

    for(index = 1; index < NUM_BLOCKS_ON_DISK - 21; index++)
    {
    	if(myBitmap.bitmap[index] != 1)
    	{
    		ret = index;
    		printf("A free block is found!\n");
    		myBitmap.bitmap[index] = 1;
    		break;
    	}

    }

    int isWriteSuccess = fwrite(&myBitmap, 1, sizeof(cs1550_bitmap), fp);
    if(isWriteSuccess == sizeof(cs1550_bitmap)) printf("bitmap updated.\n");
    
    return ret;
}

/**
 *  Update bitmap when writing to disk
 */
static int updateBitmap(FILE *fp, int index, int value)
{
	int ret = -1;

    struct cs1550_bitmap myBitmap;

    //seek to the beginning of the bitmap
    fseek(fp, -sizeof(cs1550_bitmap), SEEK_END);

    fread(&myBitmap, 1, sizeof(cs1550_bitmap), fp);

    myBitmap.bitmap[index] = value;

    int isWriteSuccess = fwrite(&myBitmap, 1, sizeof(cs1550_bitmap), fp);
    if(isWriteSuccess == sizeof(cs1550_bitmap)) 
    {	
    	printf("bitmap updated.\n");
    	ret = 0;
    }

    return ret;
}

static int allocateStartBlockPointer(FILE *fp)
{
	int blockIndex = -1;
	int newBlockIndex;

	fseek(fp, -(sizeof(cs1550_bitmap) + BLOCK_SIZE), SEEK_END);
	fread(&blockIndex, sizeof(int), 1, fp);
	fseek(fp, -(sizeof(cs1550_bitmap) + BLOCK_SIZE), SEEK_END);

	newBlockIndex = blockIndex + 1;
	fwrite(&newBlockIndex, 1, sizeof(int), fp);

	return blockIndex;	
}

/**
 *  Add directory to a free block
 */
static int addDirectoryToDisk(FILE* fp, char* dname)
{
    int ret = -1;


        int theStartBlock = findFreeBlock(fp);
        if(theStartBlock != -1)  /** -1 is returned if can't find one. **/
        {
            printf("The starting block is %d\n", theStartBlock);

            //rewind to the beginning of the .disk -> get root directory
            if(fseek(fp, 0, SEEK_SET) == 0)
            {
                    cs1550_root_directory root;
                    if(fread(&root, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
                    {
                        stpcpy(root.directories[root.nDirectories].dname, dname);
                        root.directories[root.nDirectories].nStartBlock = theStartBlock;
                        root.nDirectories += 1;
                        printf("Add directory to disk: There are %d directories on disk NOW.\n", root.nDirectories);
                        
                        //rewind again -> update root directory
                        if(fseek(fp, 0, SEEK_SET) == 0)
                        {
                            if(fwrite(&root, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
                            {
                                printf("Root directory updated.\n");
                                cs1550_directory_entry new_dir;
                                if(fseek(fp, theStartBlock * BLOCK_SIZE, SEEK_SET) == 0)
                                {
                                    if(fwrite(&new_dir, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
                                    {
                                        printf("New directory written.\n");
                                        ret = 0;
                                    }
                                }
                            }
                        }
                    }
            }
        }
        else
       	{
           	printf("Disk is FULL! Cannot allocate more blocks!\n");
        }
        
    
    return ret;
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = -ENOENT;

	memset(stbuf, 0, sizeof(struct stat));
   
	//is path the root dir?
	//If Yes, set permission
	//If else, continue checking
	if (strcmp(path, "/") == 0) 
	{

		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0;
	} 
	else 
	{	
		char dname[MAX_FILENAME + 1];   
    	char fname[MAX_FILENAME + 1];   
    	char fext[MAX_EXTENSION + 1]; 

        allocSpace(path, dname, fname, fext);

        //directory name || file name || file extension too long.
        if(dname != NULL && ((dname[MAX_FILENAME] == '\0') && (fname[MAX_FILENAME] == '\0') && (fext[MAX_EXTENSION] == '\0')))
        { 
	        FILE *fp = fopen(".disk", "rb");
	        if(fp)
	        {
	        	int directory_offset = getDirectoryOffset(fp, dname);
	        	printf("cs1550 GETATTR: directory offset is %d\n", directory_offset);
	        	if(directory_offset != -1)
	        	{
	        		// If file name specified
	        		// Set permission 666 for all file
 					if(fname[0] != '\0')
					{
						//Check if name is a regular file
						if(fseek(fp, directory_offset * BLOCK_SIZE, SEEK_SET) == 0)
						{
							cs1550_directory_entry subDirectory;
							if(fread(&subDirectory, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
							{
								int fileOffset = findFileIndexUnderDirectory(&subDirectory, fname, fext);
								if(fileOffset != -1)
								{	

									stbuf->st_mode = S_IFREG | 0666; 
                                    stbuf->st_nlink = 1;
                                    stbuf->st_size = subDirectory.files[fileOffset].fsize;
                                    res = 0;
								}
							}
						}
					}
					// Set permission 755 for all directories
					else
					{	
						stbuf->st_mode = S_IFDIR | 0755;
						stbuf->st_nlink = 2;
						res = 0;
					}
				}

	        }
	        else
	        {
	        	printf(".disk does not exist!\n");
	        	return -1;
	        }
	        fclose(fp);
    	}

	}
	return res;
}


/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	int res = -ENOENT;
	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//If it's root directory
	if (strcmp(path, "/") == 0)
	{
		cs1550_root_directory rootDirectory;
        FILE* fp = fopen(".disk", "rb");
        if(fp && (fread(&rootDirectory, 1, BLOCK_SIZE, fp) == BLOCK_SIZE))
        {
        	int numOfDirectories = rootDirectory.nDirectories;
        	int i;
        	for(i = 0;i < numOfDirectories;i++)
        	{
        		filler(buf, rootDirectory.directories[i].dname, NULL, 0);
        	}
        	res = 0;
        }
        fclose(fp);
	}
	else
	{
		char dname[MAX_FILENAME + 1];   
    	char fname[MAX_FILENAME + 1];   
    	char fext[MAX_EXTENSION + 1];
    	allocSpace(path, dname, fname, fext);

    	if(dname != NULL && ((dname[MAX_FILENAME] == '\0') && (fname[MAX_FILENAME] == '\0') && (fext[MAX_EXTENSION] == '\0')))
    	{
    		FILE *fp = fopen(".disk", "rb");
	        if(fp)
	        {
	        	int directory_offset = getDirectoryOffset(fp, dname);
	        	printf("cs1550 READDIR: directory offset is %d\n", directory_offset);
	        	if(directory_offset != -1)
	        	{
	        		//Check if name is a regular file
					if(fseek(fp, directory_offset * BLOCK_SIZE, SEEK_SET) == 0)
					{
						cs1550_directory_entry subDirectory;
						if(fread(&subDirectory, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
						{
							int numOfFiles = subDirectory.nFiles;
							int index;
							for(index = 0;index < numOfFiles;index++)
							{
								char fileNameWithExtension[FILE_NAME_LENGTH_WITH_EXTENSION];
                                strcpy(fileNameWithExtension, subDirectory.files[index].fname);
                                if(subDirectory.files[index].fext[0])
                                {
                                    strcat(fileNameWithExtension, ".");
                                    strcat(fileNameWithExtension, subDirectory.files[index].fext);
                                }
                                filler(buf, fileNameWithExtension, NULL, 0);
							}
							res = 0;
						}
					}
	        	}
	        	else
	        	{
	        		printf("cs1550 READDIR error: Directory not found!\n");
	        	}
	        }
	        else
	        {
	        	printf(".disk does not exist!\n");
	        }
	        fclose(fp);
    	}
	}
	return res;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
    (void) path;
    (void) mode;
    
    int ret = 0;
    char dname[MAX_FILENAME + 1];   
    char fname[MAX_FILENAME + 1];   
    char fext[MAX_EXTENSION + 1]; 
    allocSpace(path, dname, fname, fext);

    if(dname == NULL || dname[0] == '\0' || strlen(fname) > 0)   // if the parent directory is not root directory
    {
        printf("Cannot create directory under NON_ROOT directory!\n");
        return -EPERM;
    }

    if(strlen(dname) > MAX_FILENAME) // if the directory name exceed max filename
    {
        printf("Directory name is too LONG!\n");
        return -ENAMETOOLONG;
    }
    else
    {
        printf("Directory name is valid.\n");
        FILE* fp = fopen(".disk", "rb+");
        if(fp)
        {	
        	int directory_offset = getDirectoryOffset(fp, dname);
        	printf("cs1550 MKDIR: directory offset is %d\n", directory_offset);
            if(directory_offset != -1)   
            {
                printf("cs1550 MKDIR error: Directory already existed\n");
                return -EEXIST;
            }
            else  
            {  
            	printf("cs1550 MKDIR: Making direcotry!\n"); 
                ret = addDirectoryToDisk(fp, dname); 
        	}
        }
        else
	    {
	        printf(".disk does not exist!\n");
	    }
        fclose(fp);
    }

    return ret;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
	int res = 0;

    return res;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 * This function should add a new file to a subdirectory.
 * This function should update the .disk file appropriately with the modified directory entry structure.
 * man -s 2 mknod
 * Return 0 on success
 * -ENAMETOOLONG if the name is beyond 8.3 chars
 * -EPERM if the file is trying to be created in the root dir
 * -EEXIST if the file already exists
 * 
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	int ret = 0;


	char dname[MAX_FILENAME + 1];   
    char fname[MAX_FILENAME + 1];   
    char fext[MAX_EXTENSION + 1];
    allocSpace(path, dname, fname, fext);

    if(dname == NULL)
    {
    	return -EPERM;
    }
    else
    {
    	if(strlen(fname) > MAX_FILENAME || strlen(fext) > MAX_EXTENSION)   // if the parent directory is not root directory
	    {
	        printf("Directory name is too LONG!\n");
        	ret = -ENAMETOOLONG;
	    }
	    else
	    {
	    	FILE *fp = fopen(".disk", "rb");
	    	if(fp)
	    	{
	    		int directory_offset = getDirectoryOffset(fp, dname);
	    		printf("cs1550 MKNOD: directory offset is %d\n", directory_offset);
	       		if(directory_offset != -1)
	        	{
	        		if(fseek(fp, directory_offset * BLOCK_SIZE, SEEK_SET) == 0)
	        		{	
	        			cs1550_directory_entry subDirectory;
	        			if(fread(&subDirectory, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
	        			{	
		        			int fileOffset = findFileIndexUnderDirectory(&subDirectory, fname, fext);
		        			if(fileOffset != -1)
		        			{	
		        				printf("cs1550 MKNOD error: File already exists!\n");
		        				return -EEXIST;
		        			}
		        			else
		        			{	
		        				subDirectory.files[subDirectory.nFiles].nStartBlock = allocateStartBlockPointer(fp);
		        				strcpy(subDirectory.files[subDirectory.nFiles].fname ,fname);
								strcpy(subDirectory.files[subDirectory.nFiles].fext, fext);
								subDirectory.files[subDirectory.nFiles].fsize = 0;
								subDirectory.nFiles = subDirectory.nFiles+1;
								ret = 0;
		        			}
	        			}
	        			else
	        			{
	        				return -1;
	        			}
	        		}
	        		else
	        		{
	        			return -1;
	        		}
	        	}
	        	else
	        	{	
	        		printf("cs1550 MKNOD error: Directory not found!\n");
	        		return -1;
	        	}
	    	}
	    	else
	    	{
	    		printf(".disk does not exist!\n");
	    		return -1;
	    	}
	    	fclose(fp);
	    }
    }
    return ret;
}

/*
 * Deletes a file
 * man -s 2 unlink
 * 0  read on success
 * -EISDIR if the path is a directory
 * -ENOENT if the file is not found
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 * man -s 2 read
 * size read on success
 * -EISDIR if the path is a directory
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	int returnValue = 0;

	char dname[MAX_FILENAME + 1];   
    char fname[MAX_FILENAME + 1];   
    char fext[MAX_EXTENSION + 1];
    allocSpace(path, dname, fname, fext);

    //allocate a temp holder block for following multiple blocks reads
    cs1550_disk_block tempBlock;
	memset(&tempBlock, 0, sizeof(cs1550_disk_block));

	//make a copy of the offset 
	int tempOffset = offset;

    // If the path is a directory
    if(dname != NULL && fname == NULL) 
    {
    	return -EISDIR;
    }

    // If file name exceeds max file name
    if(strlen(fname) >= MAX_FILENAME)
    {
    	printf("Error: Exceeding MAX filename!\n");
    	return -1;
    }

    // If it's a root directory
	if (strcmp(path, "/") == 0)
	{
	  	printf("ROOT directory does not contain files!\n");
	   	return -1;
	}

	// If the starting offset is beyond buffer size
	// Or the size of the buffer is less or equal to 0 
	if(offset > size || size <= 0)
	{
		return -1;
	}

    if(dname != NULL && ((dname[MAX_FILENAME] == '\0') && (fname[MAX_FILENAME] == '\0') && (fext[MAX_EXTENSION] == '\0')))
    {
    	FILE *fp = fopen(".disk", "rb");
	    if(fp)
	    {
	    	int directory_offset = getDirectoryOffset(fp, dname);
	    	printf("cs1550 READ: directory offset is %d\n", directory_offset);
	       	if(directory_offset != -1)
	        {
	        	if(fseek(fp, directory_offset * BLOCK_SIZE, SEEK_SET) == 0)
	        	{
	        		cs1550_directory_entry subDirectory;
	        		if(fread(&subDirectory, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
	        		{
	        			int fileOffset = findFileIndexUnderDirectory(&subDirectory, fname, fext);
	        			if(fileOffset != -1)
	        			{	
	        				// Empty file, return 0 as the size
	        				if(subDirectory.files[fileOffset].fsize == 0)
	        				{
	        					return 0;
	        				}

	        				int blockNumber = subDirectory.files[fileOffset].nStartBlock;

	        				while(offset < size)
	        				{
	        					int readBlockStatus = readBlockToMem(fp, &tempBlock, blockNumber);
	        					if(readBlockStatus == -1)
	        					{
	        						printf("cs1550 READ BLOCK error!\n");
	        						return -1; 
	        					}

	        					// If the block spans multiple blocks
	        					// Set nNextblock pointer
	        					if(tempOffset > MAX_DATA_IN_BLOCK)
	        					{
	        						blockNumber = tempBlock.nNextBlock;
	        						tempOffset -= MAX_DATA_IN_BLOCK;
	        					}
	        					else
	        					{
	        						int remainingData = transferBlockToBuffer(&tempBlock, buf, tempOffset, size - offset);
	        						tempOffset = 0;
	        						if(remainingData == 0)
	        						{
	        							break;
	        						}
	        						else
	        						{
	       								blockNumber = tempBlock.nNextBlock;
	        							offset += MAX_DATA_IN_BLOCK;
	        							buf += MAX_DATA_IN_BLOCK;

	       							}
	       						}  					
	        				}

	        				returnValue = size;
	        			}
	        			else
	        			{	
	        				printf("cs1550 READ error: File not found!\n");
	        				return -1;
	        			}
	        		}
	        		else
	        		{
	        			return -1;
	        		}
	        	}
	        	else
	        	{
	        		return -1;
	        	}
	        }
	        else
	        {	
	        	printf("cs1550 READ error: Directory not found!\n");
	        	return -1;
	        }
	    }
	    else
	    {
	    	printf(".disk does not exist!\n");
	    	return -1;
	    }
	    fclose(fp);
    }
    else
    {
    	return -1;
    }

    return returnValue;
}

/* 
 * Write size bytes from buf into file starting from offset
 * man -s 2 write
 * size on success
 * -EFBIG if the offset is beyond the file size (but handle appends)
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	int returnValue = 0;

	char dname[MAX_FILENAME + 1];   
    char fname[MAX_FILENAME + 1];   
    char fext[MAX_EXTENSION + 1];
    allocSpace(path, dname, fname, fext);

    //make a copy of the offset 
	int tempOffset = offset;

    //allocate a temp holder block for following multiple blocks reads
    cs1550_disk_block tempBlock;
	memset(&tempBlock, 0, sizeof(cs1550_disk_block));

	// If the path is a directory
    if(size <= 0) 
    {
    	return -1;
    }

    //the offset is beyond the the file size
    if(offset > size)
    {
    	return -EFBIG;
    }

    // If file name exceeds max file name
    if(strlen(fname) >= MAX_FILENAME)
    {
    	printf("Error: Exceeding MAX filename!\n");
    	return -1;
    }

    // If it's a root directory
	if (strcmp(path, "/") == 0)
	{
	  	printf("ROOT directory does not contain files!\n");
	   	return -1;
	}

	if(dname[0] && (dname[MAX_FILENAME] == '\0') && (fname[MAX_FILENAME] == '\0') && (fext[MAX_EXTENSION] == '\0'))
    {
    	FILE *fp = fopen(".disk", "rb");
	    if(fp)
	    {
	    	int directory_offset = getDirectoryOffset(fp, dname);
	    	printf("cs1550 WRITE: directory offset is %d\n", directory_offset);
	       	if(directory_offset != -1)
	       	{
	       		if(fseek(fp, directory_offset * BLOCK_SIZE, SEEK_SET) == 0)
	        	{
	        		cs1550_directory_entry subDirectory;
	        		if(fread(&subDirectory, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
	        		{
	        			int fileOffset = findFileIndexUnderDirectory(&subDirectory, fname, fext);
	        			if(fileOffset != -1)
	        			{	
	        				int blockNumber = subDirectory.files[fileOffset].nStartBlock;
	        				subDirectory.files[fileOffset].fsize = size;
	        				updateDirectory(fp, &subDirectory, directory_offset);

	        				while( tempOffset >= MAX_DATA_IN_BLOCK)
							{
								blockNumber = tempBlock.nNextBlock;
								tempOffset -= MAX_DATA_IN_BLOCK;
								readBlockToMem(fp, &tempBlock, blockNumber);
							}

	        				while(offset < size)
	        				{

								int remainingData = transferBufferToBlock(&tempBlock, buf, tempOffset, size - offset);
								if(remainingData != 0 && (tempBlock.nNextBlock <= 0))
								{
									tempBlock.nNextBlock = allocateStartBlockPointer(fp);
								}

								writeBlockToDisk(fp, &tempBlock, blockNumber);
								updateBitmap(fp, blockNumber, 1);

								tempOffset = 0;
								if(remainingData == 0)
	        					{
	        						break;
	        					}
	        					else
	        					{
	       							blockNumber = tempBlock.nNextBlock;
	        						offset += MAX_DATA_IN_BLOCK;
	        						buf += MAX_DATA_IN_BLOCK;
	        						readBlockToMem(fp, &tempBlock, blockNumber);
	       						}
								
	        				}

	        				returnValue = size;
	        			}
	        			else
	        			{
	        				printf("cs1550 WRITE error: File not found!\n");
	        			}
	        		}
	        		else
	        		{
	        			return -1;
	        		}
	        	}
	        	else
	        	{
	        		return -1;
	        	}
	        }
	        else
	        {	
	        	printf("cs1550 WRITE error: Directory not found!\n");
	        	return -1;
	        }
	    }
	    else
	    {
	       	printf(".disk does not exist!\n");
	    }
	    fclose(fp);
    }
	return returnValue;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
