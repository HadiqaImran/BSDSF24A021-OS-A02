# REPORT.md

## Feature 2: ls-v1.1.0 (Long Listing)

**Q1. Difference between stat() and lstat(). When is lstat() more appropriate?**
stat() follows symbolic links and returns information about the file the link points to. lstat() does not follow the link and returns information about the link itself. ls -l should use lstat(), because it must show the link as its own entry: type `l`, size equal to the length of the target path, and the `name -> target` display. With stat() a symlink would be shown as its target, and a broken link would make the call fail.

**Q2. How can bitwise operators and macros extract file type and permissions from st_mode?**
st_mode packs the file type (high bits) and the permission bits (low bits) into one integer. Bitwise AND with a mask isolates the bits we want. For the type, `(st_mode & S_IFMT) == S_IFDIR` isolates the type field and compares it with the directory constant; macros like S_ISDIR(mode) and S_ISREG(mode) do exactly this. For permissions, `st_mode & S_IRUSR` is non-zero only if the owner-read bit is set, and the same works for S_IWUSR, S_IXUSR, S_IRGRP, S_IROTH and so on. The special bits S_ISUID, S_ISGID and S_ISVTX are tested the same way and change the x position to s, S, t or T.

## Feature 3: ls-v1.2.0 (Column Display, down then across)

**Q1. General logic for "down then across" printing. Why is a single loop insufficient?**
All names are first read into an array, and the longest name is found. The column width is the longest name plus 2 spaces of padding, the number of columns is terminal_width / column_width, and the number of rows is ceil(total_files / columns). Printing goes row by row: for row r and column c the item printed is names[r + c * rows]. The names are ordered down each column, but the terminal can only print one line at a time, left to right. A single loop over the array would print the names in order across each line (which is the horizontal, -x style layout), so consecutive names would end up next to each other instead of below each other. We need nested loops (rows outside, columns inside) and index arithmetic, and we cannot start printing until we know the total count and the longest name.

**Q2. Purpose of ioctl. Limitations of a fixed 80-column fallback?**
ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) asks the terminal driver for the current window size and fills struct winsize, where ws_col is the width in characters. This lets the program fit as many columns as the real terminal allows and adapt when the window is resized between runs. With only a fixed 80 columns, a wide terminal would waste space (fewer columns than could fit), and a narrower terminal would make long lines wrap and break the alignment. The layout also could not react to different window sizes at all.

## Feature 4: ls-v1.3.0 (Horizontal Display, -x)

**Q1. Complexity of "down then across" versus "across" printing. Which needs more pre-calculation?**
The vertical (down then across) layout is more complex. Before printing anything it must know the total number of names, the longest name, the number of columns that fit, and then the number of rows (ceiling of n / columns). Printing then needs nested loops and index arithmetic (names[r + c * rows]) to jump through the array. The horizontal layout needs only the column width and the terminal width. It is a single loop over the array that keeps a running horizontal position, prints each name padded to the column width, and starts a new line when the next name would not fit. It never needs the number of rows, because the names are printed in their natural order.

**Q2. Strategy for managing the display modes (-l, -x, default).**
A mode variable (an enum with MODE_DEFAULT, MODE_LONG and MODE_HORIZONTAL) is set in the getopt loop in main: -l sets MODE_LONG and -x sets MODE_HORIZONTAL, so if both are given the last one wins. main passes the mode to do_ls(dir, mode). Inside do_ls, if the mode is MODE_LONG it calls do_ls_long(); otherwise it reads all the names into an array and calls print_horizontal() for MODE_HORIZONTAL or print_columns() (down then across) for the default mode. Adding a new mode only needs a new constant, a new option letter and a new branch.

## Feature 5: ls-v1.4.0 (Alphabetical Sort)

**Q1. Why must all directory entries be read into memory before sorting? Drawbacks for directories with millions of files?**
readdir() returns entries in the order the filesystem stores them, which is arbitrary, not alphabetical. The alphabetically first name could be the very last entry returned, so nothing can be printed correctly until every entry has been seen; sorting needs all the data available to compare. The drawbacks with millions of files are: memory use grows with the number of files (one pointer plus one allocated string per name), sorting costs O(n log n) time, no output appears at all until reading and sorting have finished (long delay), and the program may run out of memory (malloc/realloc failure). An unsorted listing could instead be streamed entry by entry using constant memory.

**Q2. Purpose and signature of the qsort() comparison function. Why const void * arguments?**
qsort() is a generic function that does not know the element type, so the caller supplies a function that defines the order: int compare(const void *a, const void *b). It must return a negative value if a should come before b, zero if they are equal and a positive value if a should come after b. qsort() calls it with pointers to two elements of the array. Since our elements are char * (strings), the arguments are really char **, so the function casts them with *(const char **)a and then calls strcmp() on the strings. The parameters are const void * because void * lets qsort() work with any element type without knowing it, and const promises that the comparison will not modify the data being sorted.

## Feature 6: ls-v1.5.0 (Colorized Output)

**Q1. How do ANSI escape codes produce color? Show the sequence for green text.**
ANSI escape codes are special character sequences that the terminal interprets as commands instead of printing them. A sequence starts with the ESC character (\033, octal for decimal 27), followed by "[", one or more numeric parameters separated by ";", and ends with "m" (the Select Graphic Rendition command). For example, 31 selects red, 32 green, 34 blue and 35 magenta (pink) as the text color, and 7 selects reverse video. The color stays active until it is reset, so after the text we print \033[0m to return to the default. Green text: printf("\033[0;32m%s\033[0m", name); where \033[0;32m switches to green (0 = normal style, 32 = green), name is printed in green, and \033[0m resets the terminal.

**Q2. Which st_mode bits decide whether a file is executable?**
Three separate permission bits exist: S_IXUSR (0100) for the owner, S_IXGRP (0010) for the group and S_IXOTH (0001) for everyone else. A file is executable by its owner if (st_mode & S_IXUSR) is non-zero, by its group if (st_mode & S_IXGRP) is non-zero, and by others if (st_mode & S_IXOTH) is non-zero. To test for "executable by anyone", I use (st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)). Directories also have execute bits (meaning "may enter"), so the code checks for a directory first and only colors the remaining files green.
