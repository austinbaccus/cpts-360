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

int myread(int fd, char buf[], int nbytes)
{
    int logical_block, startByte, blk, avil, fileSize, offset, remain, count = 0, ibuf[BLKSIZE] = {0}, doubleibuf[BLKSIZE] = {0}; // avil is filesize-offset
    OFT *oftp = running->fd[fd];
    MINODE *mip = oftp->mptr;
    fileSize = oftp->mptr->INODE.i_size; // file size
    avil = fileSize - (oftp->offset);

    char *cq = buf;

    if (nbytes > avil) // can't read more than the file size available
    {
        nbytes = avil;
    }

    while (nbytes > 0 && avil > 0) // read until we read the amount we were supposed to
    {
        logical_block = oftp->offset / BLKSIZE;
        startByte = oftp->offset % BLKSIZE; // where we start reading in the block
        if (logical_block < 12)
        { 
            // direct block
            blk = mip->INODE.i_block[logical_block]; // blk should be a disk block now [page 348]
        }
        else if (logical_block >= 12 && logical_block < 256 + 12)
        {   
            // INDIRECT
            get_block(mip->dev, mip->INODE.i_block[12], (char*)ibuf);
            blk = ibuf[logical_block - 12]; // actual offset
        }
        else
        {   
            // DOUBLE INDIRECT
            get_block(mip->dev, mip->INODE.i_block[13], (char*)ibuf);
            logical_block = logical_block - (BLKSIZE/sizeof(int)) - 12;
            blk = ibuf[logical_block/ (BLKSIZE / sizeof(int))];

            get_block(mip->dev, blk, (char*)doubleibuf);
            blk = doubleibuf[logical_block % (BLKSIZE / sizeof(int))];
        }

        char rbuf[BLKSIZE];
        get_block(mip->dev, blk, rbuf); // read disk block into rbuf[ ]
        char *cp = rbuf + startByte;  // cp points at startByte in rbuf[]
        remain = BLKSIZE - startByte; // number of BYTEs remain in this block

        if (nbytes <= remain)
        {
            memcpy(cq, cp, nbytes); //read nbytes left
            count += nbytes;
            oftp->offset += nbytes;
            avil -= nbytes;
            remain -= nbytes;
            cq += nbytes;
            cp += nbytes;
            nbytes = 0;
        }
        else
        {
            memcpy(cq, cp, remain);
            count += remain;
            oftp->offset += remain;
            avil -= remain;
            nbytes -= remain;
            cq += remain;
            cp += remain;
            remain = 0;
        }
    }

    return count;
}

int read_file()
{
    /*
    Preparations:
    ASSUME: file is opened for RD or RW;
    ask for a fd  and  nbytes to read;
    verify that fd is indeed opened for RD or RW;
    return(myread(fd, buf, nbytes));
    */
    char * fdStr = strtok(pathname, " ");
    char * bytesStr = strtok(NULL, " ");
    int fd = atoi(fdStr);
    int bytes = atoi(bytesStr);

    if (running->fd[fd] == NULL)
    {
        return -1;
    }

    if ((running->fd[fd]->mode!=0)&&(running->fd[fd]->mode!=2))
    {
        return -1;
    }

    char buf[BLKSIZE];
    return myread(fd, buf, bytes);
}

int cat()
{
    char mybuf[BLKSIZE];
    int fd = 0;
    fd = my_open(pathname, "R"); // make sure to open for read
    printf("=======================================\n");

    while (n=myread(fd, mybuf, BLKSIZE))
    {
        char *cp = mybuf; // char ptr
        while ((cp < mybuf + strlen(mybuf)) && (*cp != 0))
        {
            if (*cp == '\\' && *(cp + 1) == 'n') // this checks if we are at the end of the buffer
            {
                printf("\n");
                cp += 2;
            }
            else
            {
                putchar(*cp);
                cp++;
            }
        }
    }

    printf("\n=======================================\n");
    close_file(fd);
}