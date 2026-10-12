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
#include <sys/ioctl.h>

extern int errno;

void do_ls(const char *dir, int mode);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char *str);

/* display modes, chosen by the command-line options */
enum { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL };

int main(int argc, char *argv[])
{
    int opt;
    int mode = MODE_DEFAULT;

    /* getopt returns each option letter in turn, and -1 when done */
    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
        case 'l':
            mode = MODE_LONG;
            break;
        case 'x':
            mode = MODE_HORIZONTAL;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l | -x] [dir...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* optind = index of the first non-option argument (first directory) */
    if (optind == argc)
    {
        do_ls(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode);
            puts("");
        }
    }
    return 0;
}

/* ===== COLUMN DISPLAY (down then across) ===== */

/* Comparison function for qsort: a and b point to elements of the array,
   and each element is a char *, so a and b are really char ** */
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

/* Reads all non-hidden names of dir into a malloc'd array of strings.
   Returns the array (NULL if dir can't be opened); sets *count and *max_len. */
char **read_names(const char *dir, int *count, int *max_len)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return NULL;
    }

    int capacity = 16, n = 0, longest = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (names == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (n == capacity)          /* array full: double its size */
        {
            capacity *= 2;
            names = realloc(names, capacity * sizeof(char *));
            if (names == NULL)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
        }
        names[n] = strdup(entry->d_name);
        if (names[n] == NULL)
        {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
        int len = (int)strlen(names[n]);
        if (len > longest)
            longest = len;
        n++;
    }
    if (errno != 0)
        perror("readdir failed");
    closedir(dp);

    qsort(names, n, sizeof(char *), compare_names);

    *count = n;
    *max_len = longest;
    return names;
}

/* Terminal width in columns; falls back to 80 if ioctl fails */
int get_term_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80;
    return w.ws_col;
}

/* Prints names in columns, filled down first and then across */
void print_columns(char **names, int n, int max_len)
{
    if (n == 0)
        return;

    int col_width = max_len + 2;
    int cols = get_term_width() / col_width;
    if (cols < 1)
        cols = 1;
    if (cols > n)
        cols = n;
    int rows = (n + cols - 1) / cols;       /* ceiling of n / cols */

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int idx = r + c * rows;         /* down-then-across index */
            if (idx >= n)
                break;
            printf("%-*s", col_width, names[idx]);
        }
        printf("\n");
    }
}

/* Prints names left to right, wrapping when the next one would not fit */
void print_horizontal(char **names, int n, int max_len)
{
    int col_width = max_len + 2;
    int term_width = get_term_width();
    int pos = 0;                            /* current horizontal position */

    for (int i = 0; i < n; i++)
    {
        if (pos > 0 && pos + col_width > term_width)
        {
            printf("\n");                   /* line is full: start a new one */
            pos = 0;
        }
        printf("%-*s", col_width, names[i]);
        pos += col_width;
    }
    if (n > 0)
        printf("\n");
}

/* Chooses the printing function based on the display mode */
void do_ls(const char *dir, int mode)
{
    if (mode == MODE_LONG)
    {
        do_ls_long(dir);
        return;
    }

    int n = 0, max_len = 0;
    char **names = read_names(dir, &n, &max_len);
    if (names == NULL)
        return;

    if (mode == MODE_HORIZONTAL)
        print_horizontal(names, n, max_len);
    else
        print_columns(names, n, max_len);

    for (int i = 0; i < n; i++)
        free(names[i]);
    free(names);
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
    int n = 0, max_len = 0;
    char **names = read_names(dir, &n, &max_len);   /* already sorted */
    if (names == NULL)
        return;

    /* ---- PASS 1: find the widest value in each column + total blocks ---- */
    int w_nlink = 1, w_owner = 1, w_group = 1, w_size = 1;
    long total_blocks = 0;
    char path[4096], owner[32], group[32];
    struct stat st;

    for (int i = 0; i < n; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
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

    /* ls counts 1024-byte blocks, st_blocks counts 512-byte blocks */
    printf("total %ld\n", total_blocks / 2);

    /* ---- PASS 2: print every line ---- */
    for (int i = 0; i < n; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }

        char perms[11];
        mode_to_string(st.st_mode, perms);
        get_owner(st.st_uid, owner, sizeof(owner));
        get_group(st.st_gid, group, sizeof(group));

        /* ctime gives "Tue Oct 13 04:05:00 2026\n"; recent files show the
           time, files older than ~6 months (or in the future) show the year */
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
               datebuf, names[i]);

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

    for (int i = 0; i < n; i++)
        free(names[i]);
    free(names);
}
