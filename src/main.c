#include "fs.h"

int main()
{
    char input[INPUT_MAX_LEN], argv[ARG_MAX_NUM][ARG_MAX_LEN];
    while (1)
    {
        initConsole();
        fgets(input, INPUT_MAX_LEN, stdin);
        int argc = 0, pos = 0, l = strlen(input), invalid = 0;
        for (int i = 0; i < l && !invalid; ++i, ++argc, pos = 0)
        {
            if (argc >= ARG_MAX_NUM)
            {
                printf("\033[1;31mERROR: argc out of range!\033[0m\n");
                invalid = 1;
                break;
            }
            while (input[i] == ' ') ++i;
            if (input[i] == '\n') break;
            while (input[i] != ' ' && input[i] != '\n')
            {
                if (pos >= ARG_MAX_LEN)
                {
                    printf("\033[1;31mERROR: some arguments too long!\033[0m\n");
                    invalid = 1;
                    break;
                }
                argv[argc][pos++] = input[i++];
            }
            argv[argc][pos] = 0;
        }
        if (argc == 0 || invalid) continue;
        else if (!strcmp(argv[0], "exit")) break;
        else if (!strcmp(argv[0], "cd"))
        {
            if (argc > 2)
                printf("\033[1;31musage: cd [directory path] !\033[0m\n");
            else if (argc == 1) node = 1;
            else cmd_cd(argv[1]);
        }
        else if (!strcmp(argv[0], "format"))
        {
            printf("Are you sure to format the disk? [y/n]\n");
            fgets(input, INPUT_MAX_LEN, stdin);
            while (strlen(input) != 2 || input[0] != 'y' && input[0] != 'n')
            {
                printf("only [y] or [n].\n");
                fgets(input, INPUT_MAX_LEN, stdin);
            }
            if (input[0] == 'y') cmd_format();
        }
        else if (!strcmp(argv[0], "stat"))
        {
            if (argc != 2)
                printf("\033[1;31musage: stat [file name/path] !\033[0m\n");
            else
            {
                int inum = path2inum(argv[1]);
                if (inum != FAIL) cmd_stat(inum);
                else printf("\033[1;31mstat: no such file or directory !\033[0m\n");
            }
        }
        else if (!strcmp(argv[0], "touch"))
        {
            if (argc != 2)
                printf("\033[1;31musage: touch [new file name/path] !\033[0m\n");
            else
                cmd_touch(argv[1]);
        }
        else if (!strcmp(argv[0], "ls"))
        {
            if (argc > 2)
                printf("\033[1;31musage: ls [directory name] !\033[0m\n");
            else if (argc == 1)
                cmd_ls(inums[node - 1]);
            else
            {
                int inum = path2inum(argv[1]);
                if (inum != FAIL) cmd_ls(inum);
            }
        }
        else if (!strcmp(argv[0], "mkdir"))
        {
            if (argc != 2)
                printf("\033[1;31musage: mkdir [new directory path] !\033[0m\n");
            else
                cmd_mkdir(argv[1]);
        }
        else if (!strcmp(argv[0], "mv"))
        {
            if (argc != 3)
                printf("\033[1;31musage: mv [file] [new path] !\033[0m\n");
            else cmd_mv(argv[1], argv[2]);
        }
        else if (!strcmp(argv[0], "rm"))
        {
            if (argc != 2)
                printf("\033[1;31musage: rm [file path] !\033[0m\n");
            else cmd_rm(argv[1]);
        }
        else if (!strcmp(argv[0], "read"))
        {
            if (argc == 4)
                cmd_read(argv[1], strtol(argv[2], NULL, 10), strtol(argv[3], NULL, 10));
            else if (argc == 3)
                cmd_read(argv[1], strtol(argv[2], NULL, 10), FAIL);
            else if (argc == 2)
                cmd_read(argv[1], 0, FAIL);
            else
                printf("\033[1;31musage: read [file name/path] ([start byte]) ([length of read])!\033[0m\n");
        }
        else if (!strcmp(argv[0], "write"))
        {
            if (argc < 3 || argc > 4)
                printf("\033[1;31musage: write [file name/path] [mode] ([start byte]) !\033[0m\n");
            else if (strlen(argv[2]) != 1 || argv[2][0] != 'a' && argv[2][0] != 'c' && argv[2][0] != 'i')
                printf("\033[1;31mmode: a-append c-cover i-insert\033[0m\n");
            else if (argc == 3 && argv[2][0] == 'i')
                printf("\033[1;31minsert mode: write [file name/path] i [start byte]\033[0m\n");
            else cmd_write(argv[1], argv[2][0], argv[2][0] == 'i' ? strtol(argv[3], NULL, 10) : 0);
        }
            // for debugging
//        else if (!strcmp(argv[0], "imap"))
//        {
//            FILE *disk = fopen(DISK_PATH, "r");
//            fseek(disk, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
//            char buf[INODE_NUM + 1];
//            fgets(buf, INODE_NUM + 1, disk);
//            for (int i = 0; i < INODE_NUM; ++i)
//                printf("%c%s", buf[i], (i + 1) % 5 == 0 ? " " : "");
//            printf("\n");
//            fclose(disk);
//        }
//        else if (!strcmp(argv[0], "dmap"))
//        {
//            FILE *disk = fopen(DISK_PATH, "r");
//            fseek(disk, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
//            char buf[DATA_BLOCK_NUM + 1];
//            fgets(buf, DATA_BLOCK_NUM + 1, disk);
//            for (int i = 0; i < DATA_BLOCK_NUM; ++i)
//                printf("%c%s", buf[i], (i + 1) % 5 == 0 ? " " : "");
//            printf("\n");
//            fclose(disk);
//        }
        else
            printf("\033[1;31mERROR: command \"%s\" not find.\033[0m\n", argv[0]);
    }
}