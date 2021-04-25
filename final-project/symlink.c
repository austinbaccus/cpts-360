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

int my_symlink(){
    char * oldpath = strtok(pathname, " ");
    char * newpath = strtok(NULL, " ");

    char parentdir[64], name[64], *cp;
    DIR *dp;
    MINODE *pip, *targetip;
    int parent;
    cp = strrchr(newpath, '/');

    if (cp == NULL)
    {
        parent = running ->cwd->ino;
        strcpy(name, newpath);
    }
    else
    {
        *(cp) = '\0';
        strcpy(parentdir, newpath);
        parent = getino(parentdir);
        strcpy(name, cp+1);
    }

    int target = creat_file(newpath);
    pip = iget(fd, parent);
    targetip = iget(fd, target);
    pip->dirty = 1;
    pip->INODE.i_links_count++;

    iput(pip);

    targetip->INODE.i_mode = 0xA1A4;
    targetip->INODE.i_size = strlen(oldpath);
    memcpy(targetip->INODE.i_block, oldpath, strlen(oldpath));
    iput(targetip);

    return 0;
}