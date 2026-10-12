/*
* Programming Assignment 02: ls-v1.1.0
* Adds the -l (long listing) option on top of v1.0.0
* Usage:
*       $ ls
*       $ ls -l
*       $ ls -l /home /etc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern int errno;

void do_ls(const char *dir);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char *str);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    /* getopt returns each option letter in turn, and -1 when done */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [dir...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* optind = index of the first non-option argument (first directory) */
    if (optind == argc)
    {
        if (long_flag)
            do_ls_long(".");
        else
            do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            if (long_flag)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/* ===== LONG LISTING ===== */

/* Converts st_mode into a string like "drwxr-xr-x" (str must hold 11 chars) */
void mode_to_string(mode_t mode, char *str)
{
    /* 1st char: file type */
    if (S_ISREG(mode))       str[0] = '-';
    else if (S_ISDIR(mode))  str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '?';

    /* owner, group, others: read / write / execute */
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    /* special bits: setuid, setgid, sticky */
    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

/* number of digits in n (used for column widths) */
int num_width(long n)
{
    int w = 1;
    while (n >= 10)
    {
        n /= 10;
        w++;
    }
    return w;
}

/* uid -> name, falling back to the number if there is no such user */
void get_owner(uid_t uid, char *buf, size_t n)
{
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        snprintf(buf, n, "%s", pw->pw_name);
    else
        snprintf(buf, n, "%u", uid);
}

/* gid -> name, falling back to the number if there is no such group */
void get_group(gid_t gid, char *buf, size_t n)
{
    struct group *gr = getgrgid(gid);
    if (gr != NULL)
        snprintf(buf, n, "%s", gr->gr_name);
    else
        snprintf(buf, n, "%u", gid);
}

void do_ls_long(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* ---- PASS 1: find the widest value in each column + total blocks ---- */
    int w_nlink = 1, w_owner = 1, w_group = 1, w_size = 1;
    long total_blocks = 0;
    char path[4096], owner[32], group[32];
    struct stat st;

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
            continue;

        get_owner(st.st_uid, owner, sizeof(owner));
        get_group(st.st_gid, group, sizeof(group));

        int w;
        if ((w = num_width((long)st.st_nlink)) > w_nlink) w_nlink = w;
        if ((w = (int)strlen(owner)) > w_owner)           w_owner = w;
        if ((w = (int)strlen(group)) > w_group)           w_group = w;
        if ((w = num_width((long)st.st_size)) > w_size)   w_size = w;
        total_blocks += st.st_blocks;
    }
    if (errno != 0)
        perror("readdir failed");

    /* ls counts 1024-byte blocks, st_blocks counts 512-byte blocks */
    printf("total %ld\n", total_blocks / 2);

    /* ---- PASS 2: go back to the start and print every line ---- */
    rewinddir(dp);
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }

        char perms[11];
        mode_to_string(st.st_mode, perms);
        get_owner(st.st_uid, owner, sizeof(owner));
        get_group(st.st_gid, group, sizeof(group));

        /* ctime gives "Tue Oct 13 04:05:00 2026\n": skip weekday, keep 12 chars */
        char *t = ctime(&st.st_mtime);
        char datebuf[16];
        time_t now = time(NULL);
        if (st.st_mtime > now || now - st.st_mtime > 15778476)
            snprintf(datebuf, sizeof(datebuf), "%.6s  %.4s", t + 4, t + 20);
        else
            snprintf(datebuf, sizeof(datebuf), "%.12s", t + 4);

        printf("%s %*lu %-*s %-*s %*ld %s %s", perms,
               w_nlink, (unsigned long)st.st_nlink,
               w_owner, owner,
               w_group, group,
               w_size, (long)st.st_size,
               datebuf, entry->d_name);

        /* symbolic link: also show where it points */
        if (S_ISLNK(st.st_mode))
        {
            char target[4096];
            ssize_t len = readlink(path, target, sizeof(target) - 1);
            if (len != -1)
            {
                target[len] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }
    closedir(dp);
}
