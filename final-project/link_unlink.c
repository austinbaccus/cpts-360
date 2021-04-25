/************* mkdri_creat.c file **************/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256];
extern int isDev;

int my_link(char * oldFile, char *newLink)
{
    int oldIno = getino(oldFile);

    if (!oldIno)
    {
        printf("Old File %s Does not exist!", oldFile);
        return -1;
    }
    MINODE *oldMip = iget(dev, oldIno);

    if (S_ISDIR(oldMip->INODE.i_mode)) {
        printf("%s is a directoy", oldFile);
        return -1;
    }

    int new_ino = getino(newLink);
    if (new_ino)
    {
        printf("Link %s Already exists!", newLink);
        return -1;
    }

    char temp[BLKSIZE];

    strcpy(temp, newLink);
    char* parent = dirname(temp);

    strcpy(temp, newLink);
    char *child = basename(temp);

    int pino = getino(parent);

    MINODE * pmip = iget(dev, pino);

    enter_name(pmip, oldMip->ino, child);

    oldMip->INODE.i_links_count++;
    oldMip->dirty = 1;
    printf("link count for %s is now %d\n", oldFile, oldMip->INODE.i_links_count);

    iput(oldMip);
    iput(pmip);
    return 1;
}

int linkProxy()
{
    char * oldFile = strtok(pathname, " ");
    char * newLink = strtok(NULL, " ");

    return my_link(oldFile, newLink);
}

int my_unlink(char* path)
{
    char temp[1024];
    strcpy(temp,path);
    char * parent = dirname(temp);
    strcpy(temp,path);
    char * child = basename(temp);

    int ino = getino(path);
    MINODE *mip = iget(dev, ino);

    if ((mip->INODE.i_mode & 0100000) != 0100000)
    {
        printf("Error: Cannot unlink NON-REG files\n");
        return -1;
    }

    int pino = getino(parent);
    MINODE * pmip = iget(dev, pino);
    pmip->dirty = 1;
    iput(pmip);
    mip->INODE.i_links_count--;
    if (mip->INODE.i_links_count > 0)
    {
        mip->dirty = 1;
    }
    else
    {
        if (!((mip->INODE.i_mode) & 0xA1FF) == 0xA1FF) 
        {
            deallocateInodeDataBlocks(mip);
        }
        mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
        mip->INODE.i_size = 0;
        mip->dirty = 1;
        idalloc(dev, mip->ino);
    }
    iput(mip);
    rm_child(pmip, child);
}

int my_unlinkProxy()
{
    return my_unlink(pathname);
}