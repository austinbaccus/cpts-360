/*********** util.c file ****************/

#include <unistd.h>

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256];

/// <summary>
/// Page 323. Reads a virtual disk block from a buffer area in memory.
/// </summary>
int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   

/// <summary>
/// Page 323. Writes a virtual disk block into a buffer area in memory.
/// </summary>
int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}

/// <summary>
/// Page 323. This function returns a pointer to the in-memory minode containing the INODE of (dev, ino). 
/// The returned minode is unique, i.e., only one copy of the INODE exists in memory. 
/// </summary>
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, disp;
  INODE *ip;

    for (i=0; i<NMINODE; i++)
    {
        mip = &minode[i];
        if (mip->dev == dev && mip->ino == ino)
        {
            mip->refCount++;
            printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
            return mip;
        }
    }
    
    for (i=0; i<NMINODE; i++)
    {
        mip = &minode[i];
        if (mip->refCount == 0)
        {
            //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
            mip->refCount = 1;
            mip->dev = dev;
            mip->ino = ino;

            // get INODE of ino to buf    
            blk  = (ino-1) / 8 + inode_start;
            disp = (ino-1) % 8;

            //printf("iget: ino=%d blk=%d disp=%d\n", ino, blk, disp);

            get_block(dev, blk, buf);
            ip = (INODE *)buf + disp;
            // copy INODE to mp->INODE
            mip->INODE = *ip;

            return mip;
        }
    }   
    printf("PANIC: no more free minodes\n");
    return 0;
}

/// <summary>
/// Page 333. (aka decFreeInodes())
/// </summary>
void decFreeBlocks(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_blocks_count--;
    put_block(dev, 1, buf);

    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_blocks_count--;
    put_block(dev, 2, buf);
}

/// <summary>
/// Page 324. Releases a used minode pointed by mip. 
/// Each minode has a ref count, which represents the number of users that are using the minode.
/// iput() decrements the refCount by 1. 
/// If the refCount is non-zero, meaning the the minode still ahs other users, the caller simply returns.
/// If the caller is the last user of the minode (refCount=0), the INODe is written back to disk if it is modified (dirty).
/// </summary>
void iput(MINODE *mip)
{
    int i, block, offset;
    char buf[BLKSIZE];
    INODE *ip;

    if (mip==0) 
        return;

    mip->refCount--;
 
    if (mip->refCount > 0) return;
    if (!mip->dirty)       return;
 
    /* write back */
    printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino); 

    block =  ((mip->ino - 1) / 8) + inode_start;
    offset =  (mip->ino - 1) % 8;

    /* first get the block containing this inode */
    get_block(mip->dev, block, buf);

    ip = (INODE *)buf + offset;
    *ip = mip->INODE;

    put_block(mip->dev, block, buf);
}

/// <summary>
/// Page 325. Tokenizes a string using '/' as the delimiter.
/// </summary>
int tokenize(char *pathname)
{
    char *s;
    strcpy(line, pathname);
    n = 0;
    s = strtok(line, "/");
    while(s)
    {
        name[n++] = s;
        s = strtok(0, "/");
    }
}

/// <summary>
/// Page 325.
/// </summary>
int search(MINODE *mip, char *name)
{
    int i;
    char *cp, temp[256], sbuf[BLKSIZE];
    DIR *dp;
    for (i=0; i<12; i++)
    { 
        // search DIR direct blocks only
        if (mip->INODE.i_block[i] == 0)
        {
            return 0;
        }
        get_block(mip->dev, mip->INODE.i_block[i], sbuf);
        dp = (DIR *)sbuf;
        cp = sbuf;
        while (cp < sbuf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%8d%8d%8u %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
            if (strcmp(name, temp)==0)
            {
                printf("found %s : inumber = %d\n", name, dp->inode);
                return dp->inode;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    return 0;
}

/// <summary>
/// Page 324. Implements the file system tree traversal algorithm. It returns the INODE number (ino) of a specified pathname.
/// </summary>
/// <returns>The INODE number (ino) of a specified pathname</returns>
int getino(char *pathname)
{
    int i, ino, blk, disp;
    INODE *ip;
    MINODE *mip;

    printf("getino: pathname=%s\n", pathname);
    if (strcmp(pathname, "/")==0)
        return 2;

    if (pathname[0]=='/')
        mip = iget(dev, 2); // TODO maybe *dev
    else
        mip = iget(running->cwd->dev, running->cwd->ino);

    tokenize(pathname);

    for (i=0; i<n; i++)
    {
        printf("===========================================\n");
        ino = search(mip, name[i]);

        if (ino==0)
        {
            iput(mip);
            printf("name %s does not exist\n", name[i]);
            return 0;
        }
        iput(mip);
        mip = iget(dev, ino);
    }
    return ino;
}

/// <summary>
/// Page 332/3.
/// </summary>
int tst_bit(char *buf, int BIT)
{
    return buf[BIT / 8] & (1 << (BIT % 8));
}

/// <summary>
/// Page 332/3.
/// </summary>
int set_bit(char *buf, int bit)
{
    int i, j;
    i = bit/8; j=bit%8;
    buf[i] |= (1 << j);
}

int clr_bit(char *buf, int bit)
{
    int i, j;
    i = bit/8; j=bit%8;
    buf[i] &= ~(1 << j);
}

/// <summary>
/// Page 332/3.*
/// </summary>
int ialloc(int dev)
{
    int  i;
    char buf[BLKSIZE];

    // read inode_bitmap block
    get_block(dev, imap, buf);

    for (i=0; i < ninodes; i++)
    {
        if (tst_bit(buf, i)==0)
        {
            set_bit(buf,i);
            put_block(dev, imap, buf);
            return i+1;
        }
    }
    return 0;
}

/// <summary>
/// Page 332/3. allocates a free disk block (number) from a device
/// </summary>
unsigned long balloc(int dev)
{
    int i;
    char buf[BLKSIZE];
    int nblocks;
    SUPER *temp;

    // get total number of blocks
    get_block(dev, 1, buf);
    temp = (SUPER *)buf;
    nblocks = temp->s_blocks_count;
    put_block(dev, 1, buf);

    get_block(dev, bmap, buf);

    for(i = 0; i < nblocks ; i++)
    {
        if(tst_bit(buf,i) == 0)
        {
            set_bit(buf,i);
            put_block(dev, bmap, buf);

            decFreeBlocks(dev);
            return i+1;
        }
    }
    return 0;
}

/// <summary>
/// Page 333.
/// </summary>
int incFreeInodes(int dev)
{
    char buf[BLKSIZE];
    // inc free inodes count in SUPER and GD
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_inodes_count++;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_inodes_count++;
    put_block(dev, 2, buf);
}

/// <summary>
/// Page 338. deallocates an inode (number). It clears the ino’s bit in the device’s inodes bitmap to 0. Then it
///           increments the free inodes count in both superblock and group descriptor by 1
/// </summary>
int idalloc(int dev, int ino)
{
    int i;
    char buf[BLKSIZE];
    // get inode bitmap block
    get_block(dev, imap, buf);
    clr_bit(buf, ino-1);
    // write buf back
    put_block(dev, imap, buf);
    // update free inode count in SUPER and GD
    incFreeInodes(dev);
}

/// <summary>
/// Page 333. Allocates a free disk block from a device. Not implemented in the book. 
/// </summary>
void bdalloc(int dev, int block)
{
    char buff[BLKSIZE];
    get_block(dev, bmap, buff);
    clr_bit(buff, block-1);
    put_block(dev, bmap, buff);
}

// ???????????????????????
void deallocateInodeDataBlocks(MINODE* mip)
{
    char bitmap[1024],dblindbuff[1024], buff[BLKSIZE];
    int i = 0;
    int j = 0;
    int indblk,dblindblk;
    unsigned long *indirect,*doubleindirect;
    get_block(dev,bmap,bitmap);
    for ( i = 0; i < 12; i++)
    {
        if (mip->INODE.i_block[i]!=0)
        {
            clr_bit(bitmap, mip->INODE.i_block[i]-1);
            mip->INODE.i_block[i]=0;
        }
        else
        {
            put_block(dev,bmap,bitmap);
            return;
        }
    }
    if (mip->INODE.i_block[i]!=0)
    {
        indblk = mip->INODE.i_block[i];
        get_block(dev,indblk,buff);
        indirect = (unsigned long *)buff;
        for (i=0;i<256;i++)
        {
            if(*indirect != 0)
            {
                clr_bit(bitmap, *indirect-1);
                *indirect = 0;
                indirect++;
            }
            else
            {
                clr_bit(bitmap, indblk-1);
                put_block(dev,indblk,buff);
                put_block(dev,bmap,bitmap);
                mip->INODE.i_block[12] = 0;
                return;
            }
        }
    }
    else
    {
        put_block(dev,bmap,bitmap);
        return;
    }
    if (mip->INODE.i_block[13]!=0)
    {
        dblindblk = mip->INODE.i_block[13];
        get_block(dev,dblindblk,dblindbuff);
        doubleindirect = (unsigned long *)dblindbuff;
        for (i=0;i<256;i++)
        {
            indblk = *doubleindirect;
            get_block(dev,indblk,buff);
            indirect = (unsigned long *)buff;
            for (j=0;j<256;j++)
            {
                if(*indirect != 0)
                {
                    clr_bit(bitmap, *indirect-1);
                    *indirect = 0;
                    indirect++;
                }
                else
                {
                    clr_bit(bitmap, indblk-1);
                    clr_bit(bitmap, dblindblk-1);
                    put_block(dev,indblk,buff);
                    put_block(dev,bmap,bitmap);
                    put_block(dev,dblindblk,dblindbuff);
                    mip->INODE.i_block[13] = 0;
                    return;
                }
                clr_bit(bitmap, indblk-1);

            }
            doubleindirect++;
            if (*doubleindirect == 0)
            {
                clr_bit(bitmap, indblk-1);
                clr_bit(bitmap, dblindblk-1);
                put_block(dev,indblk,buff);
                put_block(dev,bmap,bitmap);
                put_block(dev,dblindblk,dblindbuff);
                mip->INODE.i_block[13] = 0;
                return;
            }
        }
    }
    else
    {
        put_block(dev,bmap,bitmap);
        return;
    }
}

// reduce file size to zero
void my_truncate(MINODE *mip)
{
    deallocateInodeDataBlocks(mip);
    mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
    mip->INODE.i_size = 0;
    mip->dirty = 1;
}