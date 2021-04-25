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

int my_open(char*filename, char*modeStr)
{
    int mode;
    if (strcmp(modeStr,"R") == 0)
    {
        mode = 0;
    }
    else if (strcmp(modeStr,"W") == 0)
    {
        mode = 1;
    }
    else if (strcmp(modeStr,"RW") == 0)
    {
        mode = 2;
    }
    else if (strcmp(modeStr,"APPEND") == 0)
    {
        mode = 3;
    }
    else
    {
        printf("open: not proper mode: %s\n", modeStr);
    }

    int ino = getino(filename);
    if (!ino)
    {
        creat_file(filename);
        ino = getino(filename);
    }
    MINODE* mip = iget(dev, ino);

    if ((mip->INODE.i_mode & 0100000) != 0100000)
    {
        printf("Error: Cannot open non-regular files\n");
        return -1;
    }

    int lowFd;

    for (int i = 0; i <NFD; i++)
    {
        if (running->fd[i] == NULL)
        {
            lowFd = i;
            break;
        }

        if (running->fd[i]->mptr == mip)
        {
            if (mode > 0)
            {
                printf("open with incompatable mode: %s\n",filename);
                return -1;
            }
        }
    }
    printf("fd of %s = %d\n",filename, lowFd);
    OFT *oftp = (OFT *)malloc(sizeof(OFT));
    oftp->mode = mode;      // mode = 0|1|2|3 for R|W|RW|APPEND
    oftp->refCount = 1;
    oftp->mptr = mip;  // point at the file's minode[]

    switch(mode)
    {
        case 0 : oftp->offset = 0; // R: offset = 0
            break;
        case 1 : my_truncate(mip); // W: truncate file to 0 size
            oftp->offset = 0;
            break;
        case 2 : oftp->offset = 0; // RW: do NOT truncate file
            break;
        case 3 : oftp->offset =  mip->INODE.i_size; // APPEND mode
            break;
        default: printf("invalid mode\n");
            return -1;
    }
    running->fd[lowFd] = oftp;
    mip->INODE.i_atime = time(NULL);
    mip->dirty = 1;
    iput(mip);

    return lowFd;
}

int proxyMyOpen()
{
    char * filename = strtok(pathname, " ");
    char * modeStr = strtok(NULL, " ");
    if (modeStr == 0)
    {
        modeStr = "R";
    }

    return my_open(filename, modeStr);
}

int close_file(int fd)
{
    if (fd < 0)
    {
        printf("fd is invalid: %d",fd);
        return -1;
    }

    OFT *oftp = running->fd[fd];
    running->fd[fd] = NULL;
    oftp->refCount--;

    if (oftp->refCount > 0)
    {
        return 0;
    }

    MINODE* mip = oftp->mptr;
    iput(mip);
    return 0;
}

int closeProxy()
{
    return close_file(atoi(pathname));
}

int my_lseek(int fd, int position) {
    fd = my_open(pathname, "R");

    if (running->fd[fd] == NULL) 
    {
        return -1;
    }

    if (position <= running->fd[fd]->mptr->INODE.i_size) 
    {
        int temp = running->fd[fd]->offset;
        running->fd[fd]->offset = position;
        return temp;

    } 

    else 
    {
        return -1;
    }
}

int lseekProxy()
{
    char * fd = strtok(pathname, " ");
    char * position = strtok(NULL, " ");

    return my_lseek(atoi(fd), atoi(position));
}

int pfd()
{
    int i;
    printf("fd\tmode\toffset\tINODE\n");
    printf("--\t----\t------\t-----\n");
    for(i = 0;i<NFD;i++)
    {
        if (running->fd[i]!= NULL)
        {
            printf("%d\t", i);
            switch(running->fd[i]->mode)
            {
                case 0:
                    printf("READ\t");
                    break;
                case 1:
                    printf("WRITE\t");
                    break;
                case 2:
                    printf("R/W\t");
                    break;
                case 3:
                    printf("APPEND\t");
                    break;
                default:
                    printf("-------\t"); // this should never happen
                    break;
            }
            printf("%d\t\t[%d,%d]\n",running->fd[i]->offset, running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);
        }
    }
    return 0;
}