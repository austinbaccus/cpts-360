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

int mywrite(int fd, char buf[], int nbytes) 
{
    printf("write: ECHO=%s\n", buf);
    int logical_block, startByte, blk, remain, block12arr, block13arr, doubleblk;
    char ibuf[BLKSIZE] = {0}, doubleibuf[BLKSIZE] = {0};
    OFT *oftp = running->fd[fd];
    MINODE *mip = oftp->mptr;

    char *cq = buf;

    while (nbytes > 0) 
    {
        logical_block = oftp->offset / BLKSIZE;
        startByte = oftp->offset % BLKSIZE;  
        
        // where we start writing in the block
        if (logical_block < 12)
        {                         
            // direct block
            if (mip->INODE.i_block[logical_block] == 0) 
            {   
                // if no data block yet
                mip->INODE.i_block[logical_block] = balloc(mip->dev); // MUST ALLOCATE a block
            }
            blk = mip->INODE.i_block[logical_block]; // blk should be a disk block now
        }
        else if (logical_block >= 12 && logical_block < 256 + 12)
        { 
            // INDIRECT blocks
            if (mip->INODE.i_block[12] == 0) 
            {  
                // FIRST INDIRECT BLOCK ALLOCATION
                block12arr = mip->INODE.i_block[12] = balloc(mip->dev);
                if (block12arr == 0) 
                {
                    return 0; // this means there aren't any more dblocks :'(
                }

                get_block(mip->dev, mip->INODE.i_block[12], ibuf);
                int *ip = (int*)ibuf, p=0;  // step thru block in chunks of sizeof(int), set each ptr to 0

                for (p = 0; p < (BLKSIZE / sizeof(int)); p++)
                {
                    ip[p] = 0;
                }

                put_block(mip->dev, mip->INODE.i_block[12], ibuf); // write back to disc
                mip->INODE.i_blocks++;
            }

            int int_buf[BLKSIZE/sizeof(int)] = {0};
            get_block(mip->dev, mip->INODE.i_block[12], (char*)int_buf);
            blk = int_buf[logical_block - 12];

            if (blk==0) 
            {
                blk = int_buf[logical_block - 12] = balloc(mip->dev);
                mip->INODE.i_blocks++;
                put_block(mip->dev, mip->INODE.i_block[12], (char*)int_buf); // write back to disc
            }
        }
        else 
        { 
            // double indirect blocks
            logical_block = logical_block - (BLKSIZE/sizeof(int)) - 12;

            if (mip->INODE.i_block[13] == 0) 
            {  
                // FIRST DOUBLE INDIRECT BLOCK ALLOCATION
                block13arr = mip->INODE.i_block[13] = balloc(mip->dev);

                if (block13arr == 0)
                {
                    return 0; // no more dblocks :'(
                }

                get_block(mip->dev, mip->INODE.i_block[13], ibuf);
                int *ip = (int*)ibuf, p=0;  // step thru block in chunks of sizeof(int), set each ptr to 0

                for (p = 0; p < BLKSIZE / sizeof(int); p++) 
                {
                    ip[p] = 0;
                }

                put_block(mip->dev, mip->INODE.i_block[13], ibuf); // write back to disc
                mip->INODE.i_blocks++;
            }

            int double_int_buf[BLKSIZE/sizeof(int)] = {0};
            get_block(mip->dev, mip->INODE.i_block[13], (char*)double_int_buf);
            doubleblk = double_int_buf[logical_block/(BLKSIZE/sizeof(int))];

            if (doubleblk==0)
            {
                doubleblk = double_int_buf[logical_block/(BLKSIZE/sizeof(int))] = balloc(mip->dev);
                
                if (doubleblk == 0) 
                {
                    return 0;
                }
                
                get_block(mip->dev, doubleblk, doubleibuf);
                int *ip = (int*)doubleibuf, p=0;  

                for (p = 0; p < BLKSIZE / sizeof(int); p++) // step thru block in chunks of sizeof(int), set each ptr to 0
                {
                    ip[p] = 0;
                }

                put_block(mip->dev, doubleblk, doubleibuf); // write back to disc
                mip->INODE.i_blocks++;
                put_block(mip->dev, mip->INODE.i_block[13], (char*)double_int_buf); // write back to disc
            }

            memset(double_int_buf, 0, BLKSIZE/sizeof(int));
            get_block(mip->dev, doubleblk, (char*)double_int_buf);

            if (double_int_buf[logical_block%(BLKSIZE/sizeof(int))]==0)
            {
                blk = double_int_buf[logical_block%(BLKSIZE/sizeof(int))] = balloc(mip->dev);
                if (blk == 0) return 0;
                mip->INODE.i_blocks++;
                put_block(mip->dev, doubleblk, (char*)double_int_buf); // write back to disc
            }
        }

        char wbuf[BLKSIZE] = {0};
        /* all cases come to here : write to the data block */
        get_block(mip->dev, blk, wbuf);   // read disk block into wbuf[ ]
        char *cp = wbuf + startByte;      // cp points at startByte in wbuf[]
        remain = BLKSIZE - startByte;     // number of BYTEs remain in this block

        int amount_to_write = (remain <= nbytes) ? remain : nbytes;
        memcpy(cp, cq, amount_to_write);
        cp = cq = cp+amount_to_write;
        oftp->offset += amount_to_write;

        if (oftp->offset > mip->INODE.i_size) 
        {
            mip->INODE.i_size = oftp->offset;
        }

        nbytes -= amount_to_write;
        put_block(mip->dev, blk, wbuf);   // write wbuf[ ] to disk
    }

    mip->dirty = 1;
    printf("my_write: wrote %d char into file descriptor fd=%d\n", strlen(buf), fd);
    return nbytes;
}

int write_file() {
    char *pfd = strtok(pathname, " ");

    int writefd = pfd[0] - 48;
    if (writefd<0 || writefd>9) 
    {
        return -1;
    }
    char string[102400];
    printf("Enter the string you want to write: ");
    fgets(string, 102400, stdin);
    string[strlen(string)-1]=0;

    if (running->fd[writefd]->mode == 0)
    {
        printf("error: fd not open for write\n");
        return -1;
    }
    int nbytes = strlen(string);
    return(mywrite(writefd, string, nbytes));
}

int cp(char * source, char* dest)
{

    int fd = my_open(source, "R");
    int gd = my_open(dest, "RW");
    char buf[BLKSIZE];
    int n = 0;
    while (n = myread(fd, buf, BLKSIZE))
    {
        mywrite(gd, buf, n);
    }

    close_file(fd);
    close_file(gd);
    return 0;
}

int cpProxy()
{
    char *source = strtok(pathname, " ");
    char *dest = strtok(NULL, " ");

    return cp(source, dest);
}