#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>

// basic parameters
#define BLOCK_SIZE          4096    // 4KB
#define DISK_SIZE           65536   // 64 KB
#define INODE_SIZE          64
#define ITEM_SIZE           64
#define INODE_NUM           64
#define BITEM_NUM           64
#define IBLOCK_NUM          7
#define DIRECT_BLOCK_NUM    6
#define ROOT_INUM           0
#define DISK_PATH           "../Disk"
//#define BLOCK_NUM           16
//#define BLOCK_IN_INDIR_NUM  1365

#define DATA_BLOCK_NUM      12
#define INODE_BITMAP_BLOCK  1
#define DATA_BITMAP_BLOCK   2
#define INODE_BLOCK         3
#define DATA_BLOCK          4
#define TIME_LEN            10      // stored time length
#define SHOW_TIME_LEN       20
#define FILENAME_MAX_LEN    57

// size of variables in struct item
#define STRLEN_SIZE         4
#define INUM_BEGIN          0
#define STRLEN_BEGIN        3
#define NAME_BEGIN          7

// console parameters
#define INPUT_MAX_LEN       1024     // maximum length of input in console
#define ARG_MAX_NUM         16
#define ARG_MAX_LEN         32
#define PATH_MAX_NODE       32

// type in inode
#define IS_FREE             0
#define IS_FILE             1
#define IS_DIRE             2

// beginning byte of variables in struct inode
#define SIZE_LEN            6
#define BNUM_LEN            3
#define INUM_LEN            3
#define BLOCK_LEN           3

// beginning byte of every part in TIME
#define YEAR_BEGIN          0
#define MON_BEGIN           2
#define DAY_BEGIN           3
#define SEC_BEGIN           5

// length of every part in formatting time string
#define YEAR_LEN            2
#define DAY_LEN             2
#define SEC_LEN             5

#define SEC_PER_HOUR        3600
#define SEC_PER_MIN         60

#define FAIL                (-1)

#define min(x, y) ((x)<(y)?(x):(y))

int node = 1, inums[PATH_MAX_NODE] = {ROOT_INUM};

char *mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
char path[PATH_MAX_NODE][FILENAME_MAX_LEN] = {"root"};

typedef struct inode
{
    char mode;                          //          1  B
    char ctime[TIME_LEN + 1];           //          10 B
    char atime[TIME_LEN + 1];           //          10 B
    char mtime[TIME_LEN + 1];           //          10 B
    int size;                           //          6  B
    int bnum;                           //          3  B
    int inum;                           //          3  B
    int block[IBLOCK_NUM];              // 3B * 7 = 21 B (6 * direct block + 1 * indirect block)
} inode;                                // TOTAL    64 B

typedef struct item
{
    int inum;                           //          3  B
    int strlen;                         //          4  B
    char name[FILENAME_MAX_LEN + 1];    //          57 B
} item;                                 // TOTAL    64 B

void initConsole()
{
    printf("┌[ \033[1;34mroot");
    for (int i = 1; i < node; ++i)
        printf("/%s", path[i]);
    time_t s;
    time(&s);
    struct tm *t = localtime(&s);
    printf("\033[0m ]-( \033[1;36m%s %02d %02d:%02d:%02d 20%02d\033[0m )\n└︎─> ", mon[t->tm_mon], t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
            t->tm_year % 100);
}

void getTime(struct tm *time, char *st)
{
    time->tm_year = (st[YEAR_BEGIN] - '0') * 10 + (st[YEAR_BEGIN + 1] - '0');
    time->tm_mon = st[MON_BEGIN] - 'a';
    time->tm_mday = (st[DAY_BEGIN] - '0') * 10 + (st[DAY_BEGIN + 1] - '0');
    int sec = strtol(st + SEC_BEGIN, NULL, 10);
    time->tm_hour = sec / SEC_PER_HOUR;
    time->tm_min = sec % SEC_PER_HOUR / SEC_PER_MIN;
    time->tm_sec = sec % SEC_PER_MIN;
}

char *showTime(char *st, char *ft)          // in console
{
    struct tm t;
    getTime(&t, st);
    sprintf(ft, "%s %02d %02d:%02d:%02d 20%02d", mon[t.tm_mon], t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, t.tm_year);
    return ft;
}

char *formatTime(struct tm *pt, char *now)  // in disk
{
    sprintf(now + YEAR_BEGIN, "%0*d", YEAR_LEN, pt->tm_year % 100);
    sprintf(now + MON_BEGIN, "%c", pt->tm_mon + 'a');
    sprintf(now + DAY_BEGIN, "%0*d", DAY_LEN, pt->tm_mday);
    sprintf(now + SEC_BEGIN, "%0*d", SEC_LEN, pt->tm_hour * SEC_PER_HOUR + pt->tm_min * SEC_PER_MIN + pt->tm_sec);
    now[TIME_LEN] = 0;
    return now;
}

void saveInode(inode *in)
{
    FILE *disk = fopen(DISK_PATH, "r+");
    assert(in->inum < INODE_NUM);
    fseek(disk, INODE_BLOCK * BLOCK_SIZE + in->inum * INODE_SIZE, SEEK_SET);
    fprintf(disk, "%c", in->mode);
    fprintf(disk, "%s%s%s", in->ctime, in->atime, in->mtime);
    fprintf(disk, "%0*d%0*d%0*d", SIZE_LEN, in->size, BNUM_LEN, in->bnum, INUM_LEN, in->inum);
    for (int i = 0; i < in->bnum; ++i)
        fprintf(disk, "%0*d", BLOCK_LEN, in->block[i]);
    fclose(disk);
}

void updateMT(inode *in)
{
    time_t t;
    time(&t);
    struct tm *pt = localtime(&t);
    char now[TIME_LEN + 1];
    formatTime(pt, now);
    strncpy(in->atime, now, TIME_LEN + 1);
    strncpy(in->mtime, now, TIME_LEN + 1);
    saveInode(in);
}

void updateAT(inode *in)
{
    time_t t;
    time(&t);
    struct tm *pt = localtime(&t);
    char now[TIME_LEN + 1];
    formatTime(pt, now);
    strncpy(in->atime, now, TIME_LEN + 1);
    saveInode(in);
}

// newDataBlock return the number of the new block
// and set the corresponding bit in bitmap to 1
int newDataBlock()
{
    FILE *disk = fopen(DISK_PATH, "r+");
    assert(disk);
    fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    for (int i = 0; i < DATA_BLOCK_NUM; ++i)
        if (fgetc(disk) == '0')
        {
            fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE + i, SEEK_SET);
            fputc('1', disk), fclose(disk);
            return DATA_BLOCK + i;
        }
    fclose(disk);
    printf("\033[1;31mERROR: out of storage.\033[0m\n");
    return FAIL;
}

// return next block number via block index in inode
int nextBlock(inode *in, int i)
{
    if (in->bnum == i + 1) return FAIL;
        // ATTENTION: the next block number of block[5] isn't block[6] !!!
    else if (i < DIRECT_BLOCK_NUM - 1) return in->block[i + 1];
    else
    {
        FILE *disk = fopen(DISK_PATH, "r");
        fseek(disk, in->block[IBLOCK_NUM - 1] * BLOCK_SIZE + (i - DIRECT_BLOCK_NUM + 1) * BLOCK_LEN, SEEK_SET);
        char buf[BLOCK_LEN + 1];
        fgets(buf, BLOCK_LEN + 1, disk);
        fclose(disk);
        return strtol(buf, NULL, 10);
    }
}

void appendDataBlock(inode *in, char *s)
{
    FILE *disk = fopen(DISK_PATH, "r+");
    int cap = in->bnum * BLOCK_SIZE - in->size, len = strlen(s), block, write = 0;
    if (in->bnum > DIRECT_BLOCK_NUM)
    {
        fseek(disk, in->block[IBLOCK_NUM - 1] * BLOCK_SIZE + BLOCK_LEN * (in->bnum - IBLOCK_NUM), SEEK_SET);
        char buf[BLOCK_LEN + 1];
        fgets(buf, BLOCK_LEN + 1, disk);
        block = strtol(buf, NULL, 10);
    }
    else block = in->bnum == 0 ? 0 : in->block[in->bnum - 1];
    fseek(disk, block * BLOCK_SIZE + in->size % BLOCK_SIZE, SEEK_SET);
    fprintf(disk, "%.*s", min(cap, len), s);
    in->size += min(cap, len), write += min(cap, len);
    while (write < len)
    {
        int new = newDataBlock(), tmp;
        if (new == FAIL) return;
        if ((++in->bnum) == IBLOCK_NUM)
        {
            in->block[IBLOCK_NUM - 1] = new;
            if ((tmp = newDataBlock()) == FAIL) return;
            fseek(disk, new * BLOCK_SIZE, SEEK_SET);
            fprintf(disk, "%0*d", BLOCK_LEN, tmp);
            block = tmp;
        }
        else if (in->bnum > IBLOCK_NUM)
        {
            fseek(disk, in->block[IBLOCK_NUM - 1] * BLOCK_SIZE + BLOCK_LEN * (in->bnum - IBLOCK_NUM), SEEK_SET);
            fprintf(disk, "%0*d", BLOCK_LEN, new);
            block = new;
        }
        else
            in->block[in->bnum - 1] = block = new;
        fseek(disk, block * BLOCK_SIZE, SEEK_SET);
        fprintf(disk, "%.*s", min(BLOCK_SIZE, len - write), s + write);
        in->size += min(BLOCK_SIZE, len - write), write += min(BLOCK_SIZE, len - write);
    }
    fclose(disk);
    saveInode(in);
}

void insertDataBlock(inode *in, char *s, int st)
{
    if (st >= in->size) appendDataBlock(in, s);
    else
    {
        int block = in->block[0], i = 0, bias = st % BLOCK_SIZE, write = 0, len;
        for (; i < st / BLOCK_SIZE; block = nextBlock(in, i), ++i);
        FILE *disk = fopen(DISK_PATH, "r+");
        for (; st + write < in->size && write < strlen(s); block = nextBlock(in, i), ++i, bias = 0)
        {
            fseek(disk, block * BLOCK_SIZE + bias, SEEK_SET);
            len = min(strlen(s) - write, min(BLOCK_SIZE - bias, in->size - st - write));
            fprintf(disk, "%.*s", len, s + write);
            write += len;
        }
        fclose(disk);
        if (write < strlen(s)) appendDataBlock(in, s + write);
    }
}

void loadInode(inode *in, int inum)
{
    assert(inum < INODE_NUM);
    FILE *disk = fopen(DISK_PATH, "r");
    assert(disk);
    fseek(disk, INODE_BLOCK * BLOCK_SIZE + inum * INODE_SIZE, SEEK_SET);
    fscanf(disk, "%c", &in->mode);
    fgets(in->ctime, TIME_LEN + 1, disk);
    fgets(in->atime, TIME_LEN + 1, disk);
    fgets(in->mtime, TIME_LEN + 1, disk);
    char buf[SIZE_LEN + 1];
    fgets(buf, SIZE_LEN + 1, disk);
    in->size = strtol(buf, NULL, 10);
    fgets(buf, BNUM_LEN + 1, disk);
    in->bnum = strtol(buf, NULL, 10);
    fgets(buf, INUM_LEN + 1, disk);
    in->inum = strtol(buf, NULL, 10);
    for (int i = 0; i < in->bnum; ++i)
    {
        fgets(buf, BLOCK_LEN + 1, disk);
        in->block[i] = strtol(buf, NULL, 10);
    }
    fclose(disk);
}

void loadItem(item *it, char *str)
{
    char *buf[STRLEN_SIZE + 1];
    strncpy(buf, str + INUM_BEGIN, INUM_LEN);
    buf[INUM_LEN] = 0;
    it->inum = strtol(buf, NULL, 10);
    strncpy(buf, str + STRLEN_BEGIN, STRLEN_SIZE);
    buf[STRLEN_SIZE] = 0;
    it->strlen = strtol(buf, NULL, 10);
    strncpy(it->name, str + NAME_BEGIN, it->strlen);
    it->name[it->strlen] = 0;
}

// get inum in specified inode via file name
int getInum(int inum, char *name)
{
    if (!strlen(name)) return inum;
    inode in;
    loadInode(&in, inum);
    if (in.mode != IS_DIRE || !in.bnum) return FAIL;
    int block = in.block[0], read = 0;
    FILE *disk = fopen(DISK_PATH, "r");
    char buf[ITEM_SIZE + 1];
    item it;
    for (int i = 0; block != FAIL; block = nextBlock(&in, i), ++i)
    {
        fseek(disk, block * BLOCK_SIZE, SEEK_SET);
        for (int j = 0; j < BITEM_NUM && read < in.size; read += ITEM_SIZE, ++j)
        {
            fgets(buf, ITEM_SIZE + 1, disk);
            loadItem(&it, buf);
            if (!strcmp(it.name, name))
            {
                fclose(disk);
                return it.inum;
            }
        }
    }
    fclose(disk);
    return FAIL;
}

int addItem(int finum, int inum, char *name)
{
    inode in;
    loadInode(&in, finum);
    if (in.mode != IS_DIRE) return FAIL;
    assert(strlen(name) <= FILENAME_MAX_LEN);
    char it[ITEM_SIZE + 1];
    sprintf(it + INUM_BEGIN, "%0*d", INUM_LEN, inum);
    sprintf(it + STRLEN_BEGIN, "%0*zu", STRLEN_SIZE, strlen(name));
    sprintf(it + NAME_BEGIN, "%s", name);
    for (int i = strlen(name) + NAME_BEGIN; i < ITEM_SIZE + 1; ++i)
        it[i] = '#';
    it[ITEM_SIZE] = 0;
    appendDataBlock(&in, it);
    updateMT(&in);
    return 0;
}

int newInode(char mode, int finum)
{
    inode in;
    assert(mode == IS_DIRE || mode == IS_FILE || mode == IS_FREE);
    FILE *disk = fopen(DISK_PATH, "r+");
    assert(disk);
    fseek(disk, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    in.inum = FAIL;
    for (int i = 0; i < INODE_NUM; ++i)
        if (fgetc(disk) == '0')
        {
            fseek(disk, INODE_BITMAP_BLOCK * BLOCK_SIZE + i, SEEK_SET);
            fputc('1', disk), in.inum = i, fclose(disk);
            break;
        }
    if (in.inum == FAIL)
    {
        printf("\033[1;31mERROR: run out of inode!\033[0m\n");
        fclose(disk);
        return FAIL;
    }
    in.mode = mode;
    in.size = in.bnum = 0;
    time_t t;
    time(&t);
    struct tm *pt = localtime(&t);
    char now[TIME_LEN + 1];
    // 2B*year+1B*month+2B*day+5B*second
    formatTime(pt, now);
    strncpy(in.ctime, now, TIME_LEN + 1);
    strncpy(in.atime, now, TIME_LEN + 1);
    strncpy(in.mtime, now, TIME_LEN + 1);
    saveInode(&in);
    if (mode == IS_DIRE)
    {
        addItem(in.inum, in.inum, ".");
        if (finum > FAIL)
            addItem(in.inum, finum, "..");
    }
    return in.inum;
}

// remove the item in the finum directory and return the inum of the item
int rmItem(int finum, char *name)
{
    inode in;
    loadInode(&in, finum);
    if (in.mode != IS_DIRE || !in.bnum) return FAIL;
    int block = in.block[0], read = 0, inum = FAIL, pre;
    FILE *disk = fopen(DISK_PATH, "r+");
    char buf[ITEM_SIZE + 1];
    item it;
    for (int i = 0; block != FAIL; block = nextBlock(&in, i), ++i)
        for (int j = 0; j < BITEM_NUM && read < in.size; read += ITEM_SIZE, ++j)
        {
            fseek(disk, block * BLOCK_SIZE + j * ITEM_SIZE, SEEK_SET);
            fgets(buf, ITEM_SIZE + 1, disk);
            if (inum == FAIL)
            {
                loadItem(&it, buf);
                if (!strcmp(it.name, name))
                {
                    inum = it.inum;
                    pre = block * BLOCK_SIZE + j * ITEM_SIZE;
                }
            }
            else
            {
                fseek(disk, pre, SEEK_SET);
                fprintf(disk, "%.*s", ITEM_SIZE, buf);
                pre = block * BLOCK_SIZE + j * ITEM_SIZE;
            }
        }
    // if the removed item was at the front of the last block
    if (inum != FAIL && (in.size -= ITEM_SIZE) % BLOCK_SIZE == 0)
    {
        if ((in.bnum -= 1) == DIRECT_BLOCK_NUM)
        {
            block = in.block[IBLOCK_NUM - 1];
            fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE + block - DATA_BLOCK, SEEK_SET);
            fputc('0', disk);
        }
        block = pre / BLOCK_SIZE;
        fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE + block - DATA_BLOCK, SEEK_SET);
        fputc('0', disk);
    }
    saveInode(&in);
    fclose(disk);
    return inum;
}

int path2inum(char *p)
{
    if (!strlen(p)) return ROOT_INUM;
    int pos = 0, len = strlen(p), idx, inum = (p[0] == '/' ? ROOT_INUM : inums[node - 1]);
    char buf[FILENAME_MAX_LEN];
    while (idx = 0, pos < len)
    {
        while (p[pos] == '/') ++pos;
        if (pos == len) break;
        while (p[pos] != '/' && p[pos] != 0)
        {
            if (idx >= FILENAME_MAX_LEN) return FAIL;
            buf[idx++] = p[pos++];
        }
        buf[idx] = 0;
        if ((inum = getInum(inum, buf)) == FAIL)
            return FAIL;
    }
    return inum;
}

void delete(int inum)
{
    inode in;
    loadInode(&in, inum);
    FILE *disk = fopen(DISK_PATH, "r+");
    int block = in.bnum ? in.block[0] : FAIL, read = 0;
    if (in.mode == IS_DIRE)
    {
        item it;
        char buf[ITEM_SIZE + 1];
        for (int i = 0; block != FAIL; block = nextBlock(&in, i), ++i)
        {
            fseek(disk, block * BLOCK_SIZE, SEEK_SET);
            for (int j = 0; j < BITEM_NUM && read < in.size; ++j, read += ITEM_SIZE)
            {
                fgets(buf, ITEM_SIZE + 1, disk);
                loadItem(&it, buf);
                if (!strcmp(it.name, ".") || !strcmp(it.name, "..")) continue;
                delete(it.inum);
            }
        }
    }
    // clear data block map
    block = in.bnum ? in.block[0] : FAIL;
    for (int i = 0; block != FAIL; block = nextBlock(&in, i), ++i)
    {
        fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE + block - DATA_BLOCK, SEEK_SET);
        fputc('0', disk);
    }
    // clear secondary index data block map
    if (in.bnum > DIRECT_BLOCK_NUM)
    {
        fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE + in.block[IBLOCK_NUM - 1] - DATA_BLOCK, SEEK_SET);
        fputc('0', disk);
    }
    // clear Inode map
    fseek(disk, INODE_BITMAP_BLOCK * BLOCK_SIZE + inum, SEEK_SET);
    fputc('0', disk);
    fclose(disk);
}

void cmd_format()
{
    FILE *disk = fopen(DISK_PATH, "r+");
    assert(disk);
    ftruncate(fileno(disk), 0); // clear the disk
    char junk[DISK_SIZE + 1];
    for (int i = 0; i < DISK_SIZE; ++i)
        junk[i] = '#';
    junk[DISK_SIZE] = 0;
    fputs(junk, disk);
    // initialize bitmap
    char im[INODE_NUM], dm[DATA_BLOCK_NUM];
    for (int i = 0; i < INODE_NUM; ++i)
        im[i] = '0';
    for (int i = 0; i < DATA_BLOCK_NUM; ++i)
        dm[i] = '0';
    fseek(disk, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fprintf(disk, "%.*s", INODE_NUM, im);
    fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fprintf(disk, "%.*s", DATA_BLOCK_NUM, dm);
    fclose(disk);
    // initialize the root directory
    newInode(IS_DIRE, FAIL);
}

void cmd_stat(int inum)
{
    inode in;
    loadInode(&in, inum);
    assert(in.mode != IS_FREE);
    printf("type\t\t\t: %s\n", in.mode == IS_FILE ? "file" : "directory");
    printf("size\t\t\t: %d B\n", in.size);
    printf("inum\t\t\t: %d\n", in.inum);
    printf("block number\t\t:");
    for (int i = 0; i < in.bnum; ++i)
        printf(" %d", in.block[i]);
    printf("\n");
    char ft[SHOW_TIME_LEN + 1];
    printf("creation time\t\t: %s\n", showTime(in.ctime, ft));
    printf("last accessed time\t: %s\n", showTime(in.atime, ft));
    printf("last modified time\t: %s\n", showTime(in.mtime, ft));
}

int cmd_touch(char *p)
{
    int st = strlen(p);
    if (p[st - 1] == '/')
    {
        printf("\033[1;31musage: touch [new file (name/path) !\033[0m\n");
        return FAIL;
    }
    // find the index of last '/'
    for (st -= 1; st > 0; --st)
        if (p[st - 1] == '/') break;
    char buf[ARG_MAX_LEN];
    strncpy(buf, p, st);
    buf[st] = 0;
    int finum = st > 0 ? path2inum(buf) : inums[node - 1], inum = FAIL;
    if (finum != FAIL && getInum(finum, p + st) == FAIL)
        addItem(finum, inum = newInode(IS_FILE, FAIL), p + st);
    else
        printf("\033[1;31mtouch: \"%s\"︎ is already existed !\033[0m\n", p + st);
    return inum;
}

void cmd_ls(int inum)
{
    inode crt, tmp;
    loadInode(&crt, inum);
    if (crt.mode != IS_DIRE)
    {
        printf("\033[1;31mls:    ⬆︎ is not a directory !\033[0m\n");
        return;
    }
    if (!crt.bnum) return;
    printf("\033[1;35m%-*s %-*s %-*s %-*s\033[0m\n", ARG_MAX_LEN, "File Name",
            3 * TIME_LEN, "Modification Time", 2 * SIZE_LEN, "Size(B)", 2 * SIZE_LEN, "Type");
    int block = crt.block[0], read = 0;
    FILE *disk = fopen(DISK_PATH, "r");
    char buf[ITEM_SIZE + 1], tm[SHOW_TIME_LEN + 1];
    item it;
    for (int i = 0; block != FAIL; block = nextBlock(&crt, i), ++i)
    {
        fseek(disk, block * BLOCK_SIZE, SEEK_SET);
        for (int j = 0; j < BITEM_NUM && read < crt.size; read += ITEM_SIZE, ++j)
        {
            fgets(buf, ITEM_SIZE + 1, disk);
            loadItem(&it, buf);
            loadInode(&tmp, it.inum);
            printf("%-*s %-*s %-*d %-*s\n", ARG_MAX_LEN, it.name, 3 * TIME_LEN, showTime(tmp.atime, tm),
                    2 * SIZE_LEN, tmp.size, 2 * SIZE_LEN, tmp.mode == IS_DIRE ? "DIR" : "FILE");
        }
    }
    fclose(disk);
    updateAT(&crt);
}

void cmd_mkdir(char *p)
{
    int st, end = strlen(p);
    while (end > 0 && p[end - 1] == '/') --end;
    st = end - 1;
    while (st > 0 && p[st - 1] != '/') --st;
    if (st + 1 == end)
    {
        printf("\033[1;31musage: mkdir [new directory name/path] !\033[0m\n");
        return;
    }
    char new[ARG_MAX_LEN], buf[ARG_MAX_LEN];
    for (int i = st; i <= end; ++i)
        new[i - st] = p[i];
    strncpy(buf, p, st);
    buf[st] = 0;
    int inum = st > 0 ? path2inum(buf) : inums[node - 1];
    if (inum != FAIL && getInum(inum, new) == FAIL)
        addItem(inum, newInode(IS_DIRE, inum), new);
    else
        printf("\033[1;31mmkdir: \"%s\"︎ is already existed !\033[0m\n", new);
}

void cmd_cd(char *p)
{
    int pos = 0, len = strlen(p), idx, inum = (p[0] == '/' ? (node = 1, ROOT_INUM) : inums[node - 1]);
    char buf[ARG_MAX_LEN];
    while (idx = 0, pos < len)
    {
        while (p[pos] == '/') ++pos;
        if (pos == len) break;
        while (p[pos] != '/' && p[pos] != 0)
            buf[idx++] = p[pos++];
        buf[idx] = 0;
        if ((inum = getInum(inum, buf)) == FAIL)
        {
            printf("\033[1;31mcd: \"%s\" is not [a directory/existed] !\033[0m\n", buf);
            return;
        }
        if (!strcmp(buf, ".")) continue;
        else if (!strcmp(buf, "..")) node -= 1;
        else
        {
            inode in;
            loadInode(&in, inum);
            if (in.mode == IS_DIRE)
                inums[node] = inum, strcpy(path[node++], buf);
            else
                printf("\033[1;31mcd: \"%s\" is not a directory !\033[0m\n", buf);
        }
    }
}

void cmd_mv(char *org, char *dst)
{
    int oinum, dinum, osl = strlen(org), dsl = strlen(dst), finum;
    while (osl > 0 && org[osl - 1] == '/') org[--osl] = 0;
    while (osl > 0 && org[osl - 1] != '/') --osl;
    finum = osl > 0 ? (org[osl - 1] = 0, path2inum(org)) : inums[node - 1];
    if (finum == FAIL || (oinum = getInum(finum, org + osl)) == FAIL)
    {
        printf("\033[1;31mmv: no such file or directory !\033[0m\n");
        return;
    }
    if ((dinum = path2inum(dst)) != FAIL)
    {
        if (getInum(dinum, org + osl) != FAIL || addItem(dinum, oinum, org + osl) == FAIL)
            printf("\033[1;31mmv: \"%s\" is already existed !\033[0m\n", org + osl);
        else rmItem(finum, org + osl);
    }
    else
    {
        dsl = strlen(dst);
        while (dsl > 0 && dst[dsl - 1] != '/') --dsl;
        dinum = dsl > 0 ? (dst[dsl - 1] = 0, path2inum(dst)) : inums[node - 1];
        if (dinum != FAIL && addItem(dinum, oinum, dst + dsl) != FAIL)
            rmItem(finum, org + osl);
        else printf("\033[1;31mmv: destination path is not existed !\033[0m\n", org + osl);
    }
}

void cmd_rm(char *p)
{
    int finum, inum, sl = strlen(p);
    while (sl > 0 && p[sl - 1] == '/') p[--sl] = 0;
    while (sl > 0 && p[sl - 1] != '/') --sl;
    finum = sl > 0 ? (p[sl - 1] = 0, path2inum(p)) : inums[node - 1];
    if (finum == FAIL || (inum = rmItem(finum, p + sl)) == FAIL)
    {
        printf("\033[1;31mrm: no such file or directory !\033[0m\n");
        return;
    }
    else delete(inum);
}

void cmd_read(char *p, int st, int len)
{
    int inum = path2inum(p);
    if (inum == FAIL)
    {
        printf("\033[1;31mread: no such file or directory !\033[0m\n");
        return;
    }
    inode in;
    loadInode(&in, inum);
    if (in.mode == IS_DIRE)
    {
        printf("\033[1;31mread: %s is a directory !\033[0m\n", p);
        return;
    }
    if (st >= in.size || !in.size) return;
    len = len == FAIL ? in.size : len;
    int block = in.block[0], bias = st % BLOCK_SIZE, rem =
            st + len > in.size ? in.size - st : len, i, flag = 0, idx = 0;
    for (i = 0; i < st / BLOCK_SIZE; block = nextBlock(&in, i), ++i);
    FILE *disk = fopen(DISK_PATH, "r");
    char buf[BLOCK_SIZE + 1], c;
    for (; rem > 0; block = nextBlock(&in, i), ++i, bias = flag = idx = 0)
    {
        fseek(disk, block * BLOCK_SIZE + bias, SEEK_SET);
//        fgets(buf, min(rem, BLOCK_SIZE - bias) + 1, disk);
        for (int j = 0; j < min(rem, BLOCK_SIZE - bias); ++j, buf[idx] = 0)
        {
            c = fgetc(disk);
            if (c == '\\' && flag == 0) flag = 1;
            else if (c == '\\' && flag == 1) buf[idx++] = '\\', flag = 0;
            else if (c == 'n' && flag == 1) buf[idx++] = '\n', flag = 0;
            else if (flag == 1) buf[idx++] = '\\', buf[idx++] = c, flag = 0;
            else buf[idx++] = c, flag = 0;
        }
        rem -= min(rem, BLOCK_SIZE - bias);
        printf("%s", buf);
    }
    printf("\n");
    updateAT(&in);
}

void cmd_write(char *p, char m, int st)
{
    int inum = path2inum(p), flag = FAIL, write = 0;
    if (inum == FAIL && (inum = cmd_touch(p)) == FAIL)
    {
        printf("\033[1;31mwrite: no such directory !\033[0m\n");
        return;
    }
    assert(inum != FAIL);
    char buf[INPUT_MAX_LEN + 1];
    inode in;
    loadInode(&in, inum);
    if (in.mode == IS_DIRE)
    {
        printf("\033[1;31mwrite: can't write a directory !\033[0m\n");
        return;
    }
    switch (m)
    {
        case 'a':
            while (fgets(buf, INPUT_MAX_LEN, stdin), strcmp(buf, "EOF\n"))
                buf[strlen(buf) - 1] = 0, appendDataBlock(&in, buf);
            break;
        case 'c':
            delete(inum);
            in.bnum = in.size = 0;
            saveInode(&in);
            FILE *disk = fopen(DISK_PATH, "r+");
            fseek(disk, INODE_BITMAP_BLOCK * BLOCK_SIZE + inum, SEEK_SET);
            fputc('1', disk);
            fclose(disk);
            while (fgets(buf, INPUT_MAX_LEN, stdin), strcmp(buf, "EOF\n"))
                buf[strlen(buf) - 1] = 0, appendDataBlock(&in, buf);
            break;
        case 'i':
            while (fgets(buf, INPUT_MAX_LEN, stdin), strcmp(buf, "EOF\n"))
                buf[strlen(buf) - 1] = 0, insertDataBlock(&in, buf, st + write), write += strlen(buf);
            break;
    }
    updateMT(&in);
}