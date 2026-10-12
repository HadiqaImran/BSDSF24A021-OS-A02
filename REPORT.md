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
