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

int mymkdir(MINODE *pip, char *name)
{
    int ino = ialloc(dev);
    if (isDev){printf("ino: %d", ino);}
    int bno = balloc(dev);
    if (isDev){printf("bno: %d", bno);}
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x41ED;		// OR 040755: DIR type and permissions
    ip->i_uid  = running->uid;	// Owner uid
    ip->i_gid  = running->gid;	// Group Id
    ip->i_size = BLKSIZE;		// Size in bytes
    ip->i_links_count = 2;	        // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
    ip->i_blocks = 2;                	// LINUX: Blocks count in 512-byte chunks
    ip->i_block[0] = bno;             // new DIR has one data block
    for (int i =1; i < 16; i++) 
    {
        ip->i_block[i] = 0;
    }
    mip->dirty = 1;
    iput(mip);

    char buf[BLKSIZE];
    char *cp;
    get_block(dev, bno, buf);
    dp=(DIR*)buf;
    cp=buf;

    dp->inode=ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    dp->file_type=(u8)EXT2_FT_DIR;
    dp->name[0] ='.';
    cp+=dp->rec_len;
    dp=(DIR*)cp;

    dp->inode= pip->ino;
    dp->rec_len=BLKSIZE-12;
    dp->name_len=2;
    dp->file_type=(u8)EXT2_FT_DIR;
    dp->name[0]=dp->name[1]='.';

    put_block(dev,bno, buf);
    enter_name(pip, ino, name);
    pip->dirty = 1;
    iput(pip);
    return 1;
}

/// <summary>
/// Page 332. Creates an empty directory with a data block containing the 
/// default . and .. entries.
/// </summary>
int mk_dir()
{
    MINODE *mip;
    int dev;
    if (pathname[0] == '/') 
    {
        mip = root;
    }
    else
    {
        mip = running->cwd;
    }
    dev = mip->dev;

    char temp[BLKSIZE];
    char parent[BLKSIZE];
    char child[BLKSIZE];

    strcpy(temp, pathname);
    strcpy(parent, dirname(temp));

    strcpy(temp, pathname);
    strcpy(child, basename(temp));

    int pino = getino(parent);
    MINODE * pip = iget(dev, pino);

    if(!S_ISDIR(pip->INODE.i_mode))
    {
        printf("error: not a directory\n");
        return 0;
    }
    if (getino(pathname) != 0)
    {
        printf("directory already exists!\n");
        return 0;
    } 

    // increment parent's link count by 1
    mymkdir(pip, child);
    pip->INODE.i_links_count++;
    pip->INODE.i_atime = pip->INODE.i_ctime = pip->INODE.i_mtime = time(0L);
    pip->dirty = 1;

    iput(pip);
    return 1;
}

int my_creat(MINODE *pip, char *name)
{
    int ino = ialloc(dev);
    if (isDev) 
    { 
        printf("ino: %d", ino);
    }
    int bno = balloc(dev);
    if (isDev) 
    {
        printf("bno: %d", bno);
    }
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x81A4; // OR 0100644: FILE type
    ip->i_uid  = running->uid; // Owner uid
    ip->i_gid  = running->gid; // Group Id
    ip->i_size = 0; // Size in bytes
    ip->i_links_count = 1; // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
    ip->i_blocks = 0; // LINUX: Blocks count in 512-byte chunks
    ip->i_block[0] = bno; // new DIR has one data block
    for (int i =1; i < 16; i++) 
    {
        ip->i_block[i] = 0;
    }
    mip->dirty = 1;
    iput(mip);
    enter_name(pip, ino, name);
    return ino;
}

/// <summary>
/// Page 336. Creates an empty regular file.
/// </summary>
int creat_file(char *filename)
{
    MINODE *mip;
    int dev;
    if (filename[0] == '/') {
        mip = root;
    }
    else
    {
        mip = running->cwd;
    }
    dev = mip->dev;

    char temp[BLKSIZE];
    char parent[BLKSIZE];
    char child[BLKSIZE];

    strcpy(temp, filename);
    strcpy(parent, dirname(temp));

    strcpy(temp, filename);
    strcpy(child, basename(temp));

    int pino = getino(parent);
    MINODE * pip = iget(dev, pino);

    if (!S_ISDIR(pip->INODE.i_mode))
    {
        printf("error: not a directory\n");
        return 0;
    }
    if (getino(filename) != 0)
    {
        printf("file already exists!\n");
        return 0;
    }
    my_creat(pip, child);

    pip->INODE.i_atime=time(0L);
    pip->dirty = 1;

    iput(pip);
    return 1;
}

int create_fileProxy()
{
    return creat_file(pathname);
}

int enter_name(MINODE *pip, int myino, char *myname)
{
    int i = 0;
    char buf[BLKSIZE];

    for(i =0; i< 12; i++)
    {
        if (pip->INODE.i_block[i] == 0)
        {
            break;
        }
        get_block(dev, pip->INODE.i_block[i], buf);
        dp=(DIR *)buf;
        char * cp = buf;
        int need_length = 4*( (8 + strlen(myname) + 3)/4 );
        if(isDev){printf("length: %d\n", need_length);}

        get_block(pip->dev, pip->INODE.i_block[i], buf);

        dp= (DIR*)buf;
        cp = buf;
        // step to LAST entry in block: int blk = parent->INODE.i_block[i];

        printf("step to LAST entry in data block %d\n", pip->INODE.i_block[i]);
        while (cp + dp->rec_len < buf + BLKSIZE)
        {
            /****** Technique for printing, compare, etc.******
            c = dp->name[dp->name_len];
            dp->name[dp->name_len] = 0;
            printf("%s ", dp->name);
            dp->name[dp->name_len] = c;
            **************************************************/

            cp += dp->rec_len;
            dp = (DIR *)cp;
        }

        // dp NOW points at last entry in block
        printf("last_entry: %s\n", dp->name);
        cp = (char *)dp;
        int IDEAL_LEN = 4*( (8 + dp->name_len + 3)/4 );
        int remain = dp->rec_len - IDEAL_LEN;
        printf("remain: %d\n", remain);
        if (remain >= need_length)
        {
            dp->rec_len = IDEAL_LEN;
            cp += dp->rec_len;
            dp = (DIR*)cp;

            dp->inode = myino;
            dp->rec_len = 1024 - ((u32)cp-(u32) buf);
            printf("rec_len: %d\n", dp->rec_len);
            dp->name_len = strlen(myname);
            dp->file_type = EXT2_FT_DIR;
            strcpy(dp->name, myname);
            put_block(dev,pip->INODE.i_block[i],buf);
            return 1;
        }
    }
    printf("Number of Data Blocks: %d\n", i);

    dp->inode=myino;
    dp->rec_len = BLKSIZE;
    dp->name_len=strlen(myname);
    dp->file_type=EXT2_FT_DIR;
    strcpy(dp->name, myname);

    put_block(dev, pip->INODE.i_block[i], buf);
    return 1;
}