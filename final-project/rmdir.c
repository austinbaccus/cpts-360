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

void my_rmdir()
{
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    if (!S_ISDIR(mip->INODE.i_mode)) 
    {
        printf("%s is not a directory", pathname);
        return;
    }
    if (mip->refCount > 2)
    {
        printf("ref count is %d so it is not available\n", mip->refCount);
        return;
    }
    if (!isEmpty(mip))
    {
        printf("DIR not very empty\n");
        iput(mip);
        return;
    }

    for (int i=0; i<12; i++)
    {
        if (mip->INODE.i_block[i] == 0)
        {
            continue;
        }
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }

    idalloc(mip->dev, mip->ino);
    iput(mip); // which clears mip->refCount = 0

    int pino = find_ino(mip, &ino);
    MINODE *pmip = iget(mip->dev, pino);

    rm_child(pmip,pathname);
    pmip->INODE.i_links_count--;
    pmip->dirty = 1;
    iput(pmip);
}

void rm_child(MINODE *pmip, char *name)
{
    INODE* pip = &pmip->INODE;
    char sbuf[BLKSIZE], temp[256];
    DIR *dp, *startDp;
    char  *finalCp, *cp;
    int first, last;
    DIR *predDir;

    // assume DIR at most 12 direct blocks
    for (int i=0; i < 12; i++)
    {  
        if (pip->i_block[i] == 0)
        {
            return;
        }

        printf("i=%d i_block[%d]=%d\n",i,i,pip->i_block[i]);
        get_block(dev, pip->i_block[i], sbuf);

        dp = (DIR *)sbuf;
        cp = sbuf;
        int total_length = 0;
        while(cp < sbuf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            total_length += dp->rec_len;
            temp[dp->name_len] = 0;
            if (!strcmp(temp, name))
            {
                if (cp == sbuf && cp + dp->rec_len == sbuf + BLKSIZE) //first
                {
                    memset(sbuf, '\0', BLKSIZE);
                    bdalloc(dev, ip->i_block[i]);

                    pip->i_size -=BLKSIZE;
                    while(pip->i_block[i+i] && i+1 < 12)
                    {
                        get_block(dev, pip->i_block[i], sbuf);
                        put_block(dev, pip->i_block[i-1], sbuf);
                        i++;
                    }
                }
                else if(cp+dp->rec_len == sbuf + BLKSIZE) //last
                {
                    predDir->rec_len +=dp->rec_len;
                    put_block(dev, pip->i_block[i], sbuf);
                }
                else //middle
                {
                    int removed_length = dp->rec_len;
                    char* cNext = cp + dp->rec_len;
                    DIR* dNext = (DIR *)cNext;
                    while(total_length + dNext->rec_len < BLKSIZE)
                    {
                        total_length += dNext->rec_len;
                        int next_length = dNext->rec_len;
                        dp->inode = dNext->inode;
                        dp->rec_len = dNext->rec_len;
                        dp->name_len = dNext->name_len;
                        strncpy(dp->name, dNext->name, dNext->name_len);
                        cNext += next_length;
                        dNext = (DIR *)cNext;
                        cp+= next_length;
                        dp = (DIR *)cp;
                    }
                    dp->inode = dNext->inode;
                    // add removed rec_len to the last entry of the block
                    dp->rec_len = dNext->rec_len + removed_length;
                    dp->name_len = dNext->name_len;
                    strncpy(dp->name, dNext->name, dNext->name_len);
                    put_block(dev, pip->i_block[i], sbuf); // save
                    pmip->dirty = 1;
                    return;
                }
                pmip->dirty=1;
                iput(pmip);
                return;
            }
            predDir = dp;
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
}