/************* cd_ls_pwd.c file **************/

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

#define OWNER  000700
#define GROUP  000070
#define OTHER  000007

/// <summary>
/// Change directory.
/// </summary>
/// <returns>The result of the operation. -1 means that it couldn't change directories.</returns>
int change_dir()
{
    printf("cd");
    char temp[EXT2_NAME_LEN];
    MINODE *newip;
    int ino, dev;

    if (pathname[0] == 0)
    {
        iput(running->cwd);
        running->cwd = iget(dev, 2);
        return -1;
    }

    if (pathname[0] == '/')
        dev = root->dev;
    else
        dev = running->cwd->dev;
    strcpy(temp, pathname);
    printf("temp: %s", temp);
    ino = getino(temp);
    if (!ino)
    {
        printf("no such directory\n");
        return -1;
    }

    printf("dev=%d ino=%d\n", dev, ino);
    newip = iget(dev, ino);
    printf("mode=%4x\n", newip->INODE.i_mode);
    if (!S_ISDIR(newip->INODE.i_mode))
    {
        printf("%s is not a directory\n", pathname);
        iput(newip);
        return -1;
    }

    iput(running->cwd);
    running->cwd = newip;
    printf("after cd : cwd = [%d %d]\n", running->cwd->dev, running->cwd->ino);
    return 0;
}

int ls_file(MINODE *mip, char *name)
{
    char *t1 = "xwrxwrxwr-------";
    char *t2 = "----------------";
    u16 mode, mask;
    mode = mip->INODE.i_mode;
    if ((mode & 0xF000) == 0x8000) // if (S_ISREG())
        printf("%c",'-');
    if ((mode & 0xF000) == 0x4000) // if (S_ISDIR())
        printf("%c",'d');
    if ((mode & 0xF000) == 0xA000) // if (S_ISLNK())
        printf("%c",'l');
    for (int i=8; i >= 0; i--)
    {
        if (mode & (1 << i)) // print r|w|x
            printf("%c", t1[i]);
        else
            printf("%c", t2[i]);
    }
    printf("%4d", mip->INODE.i_links_count);
    printf("%4d", mip->INODE.i_uid);
    printf("%4d", mip->INODE.i_gid);
    printf("\t");
    printf("%8d", mip->INODE.i_size);
    char filetime[256];
    time_t time = (time_t)mip->INODE.i_mtime;
    strcpy(filetime, ctime(&time));
    filetime[strlen(filetime)-1] = 0;
    printf("\t%s", filetime);
    printf("\t%s", name);
    if (S_ISLNK(mode))
        printf(" -> %s", (char *)mip->INODE.i_block);
    printf("\n");
}

int ls_dir(MINODE *mip)
{
    char buf[BLKSIZE], temp[256];
    DIR *dp;
    char *cp;
    MINODE *dip;

    for (int i = 0; i < 12; i++)
    {
        printf("block: %d\n", mip->INODE.i_block[i]);
        if (mip->INODE.i_block[i] == 0)
            return 0;
        get_block(mip->dev, mip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;
        while (cp < buf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            dip = iget(dev, dp->inode);
            ls_file(dip, temp);
            iput(dip);

            cp+= dp->rec_len;
            dp = (DIR *)cp;
        }
    }
}

int find_ino(MINODE *mip, u32 *myino)
{
    char buf[BLKSIZE], *cp;
    DIR *dp;

    get_block(mip->dev, mip->INODE.i_block[0], buf);
    cp = buf;
    dp = (DIR *)buf;
    *myino = dp->inode;
    cp += dp->rec_len;
    dp = (DIR *)cp;
    return dp->inode;
}

int findmyname(MINODE *parent, u32 myino, char *myname)
{
    int i;
    char buf[BLKSIZE], temp[256], *cp;
    DIR *dp;
    MINODE *mip = parent;
    //SEARCH FOR FILENAME
    for (i=0; i < 12; i++)
    {  
        // assume DIR at most 12 direct blocks
        if (mip->INODE.i_block[i] == 0)
            return -1;
        get_block(dev, mip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        while(cp < buf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%4d %4d %4d %s\n",
                   dp->inode, dp->rec_len, dp->name_len, temp);
            if(dp->inode == myino)
            {
                strncpy(myname, dp->name, dp->name_len);
                myname[dp->name_len] = 0;
                //printf("found %s : ino = %d\n",myname,dp->inode);
                return 0;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    return -1;
}

/// <summary>
/// Prints out minode information.
/// </summary>


int list_file()
{
     MINODE *mip;
     u16 mode;
     int dev, ino;
     if (pathname[0] == 0)
     {
         return ls_dir(running->cwd);
     }
     dev = root->dev;
     ino = getino(pathname);
     if (ino == 0)
     {
         printf("file does not exist: %s\n", pathname);
         return -1;
     }
     mip = iget(dev, ino);
     mode = mip->INODE.i_mode;
     if (!S_ISDIR(mode))
         ls_file(mip, basename(name[0]));
     else
         ls_dir(mip);
     iput(mip);
}

void rpwd(MINODE *wd)
{
    char myname[256];
    MINODE *parent;
    u32 myino, parentino;
    if (wd==root) return;
    parentino = find_ino(wd, &myino);
    parent = iget(dev, parentino);

    findmyname(parent, myino, myname);
    rpwd(parent);
    iput(parent);
    printf("/%s", myname);
    return;
}

void pwd(MINODE *wd)
{
    if (wd == root)
        printf("/");
    else
        rpwd(wd);
    printf("\n");
}

/// <summary>
/// Page 329. Writes modified minodes to disk.
/// </summary>
void myQuit()
{
    int i;
    MINODE *mip;
    for (i=0; i<NMINODE; i++){
        mip = &minode[i];
        if (mip->refCount > 0)
            iput(mip);
    }
    exit(0);
}

int isEmpty(MINODE *mip)
{
    int i;
    char buf[BLKSIZE], namebuf[256], *cp;

    // more than 2 links?
    if(mip->INODE.i_links_count > 2)
        return 0;

    // only 2 links?
    if(mip->INODE.i_links_count == 2)
    {
        // cycle through each direct block to check...
        for(i = 0; i <= 11; i++)
        {
            if(mip->INODE.i_block[i])
            {
                get_block(mip->dev, mip->INODE.i_block[i], buf);
                cp = buf;
                dp = (DIR *)buf;

                while(cp < &buf[BLKSIZE])
                {
                    strncpy(namebuf, dp->name, dp->name_len);
                    namebuf[dp->name_len] = 0;

                    // if stuff exists, this directory isn't empty :(
                    if(strcmp(namebuf, ".") && strcmp(namebuf, ".."))
                        return 0;

                    cp+=dp->rec_len;
                    dp=(DIR *)cp;
                }
            }
        }
        return 1;
    }
    return -1;
}

int my_stat()
{
    struct stat myStat2;
    struct stat *myStat = &myStat2;
    int ino = getino(pathname);
    MINODE *mip = iget(fd, ino);
    myStat->st_dev = fd;
    memcpy(&myStat->st_ino, &ino, sizeof(ino_t));
    memcpy(&myStat->st_mode, &mip->INODE.i_mode, sizeof(mode_t));
    memcpy(&myStat->st_nlink, &mip->INODE.i_links_count, sizeof(nlink_t));
    memcpy(&myStat->st_uid, &mip->INODE.i_uid, sizeof(uid_t));
    memcpy(&myStat->st_gid, &mip->INODE.i_gid, sizeof(gid_t));
    memcpy(&myStat->st_size, &mip->INODE.i_size, sizeof(off_t));
    myStat->st_blksize = 1024;
    memcpy(&myStat->st_blocks, &mip->INODE.i_blocks, sizeof(blkcnt_t));
    memcpy(&myStat->st_atime, &mip->INODE.i_atime, sizeof(time_t));
    memcpy(&myStat->st_mtime, &mip->INODE.i_mtime, sizeof(time_t));
    memcpy(&myStat->st_ctime, &mip->INODE.i_ctime, sizeof(time_t));
    printf("dev: %d\t", (int)myStat->st_dev);
    printf("ino: %u\t\t", (int)myStat->st_ino);
    printf("mode: %u\t", (int)myStat->st_mode);
    printf("nlink: %lu\t", (int)myStat->st_nlink);
    printf("uid: %u\t", (int)myStat->st_uid);
    printf("\n");
    printf("gid: %u\t", (int)myStat->st_gid);
    printf("size: %d\t", (int)myStat->st_size);
    printf("blksize: %d\t", (int)myStat->st_blksize);
    printf("blocks: %lu\t", (int)myStat->st_blocks);
    char *time_string = ctime(&myStat->st_ctime);
    printf("\nctime: %s", time_string);
    time_string = ctime(&myStat->st_atime);
    printf("atime: %s", time_string);
    time_string = ctime(&myStat->st_mtime);
    printf("mtime: %s", time_string);
    printf("\n");

    iput(mip);
    return 0;
}