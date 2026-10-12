# REPORT.md

## Feature 2: ls-v1.1.0 (Long Listing)

**Q1. Difference between stat() and lstat(). When is lstat() more appropriate?**
stat() follows symbolic links and returns information about the file the link points to. lstat() does not follow the link and returns information about the link itself. ls -l should use lstat(), because it must show the link as its own entry: type `l`, size equal to the length of the target path, and the `name -> target` display. With stat() a symlink would be shown as its target, and a broken link would make the call fail.

**Q2. How can bitwise operators and macros extract file type and permissions from st_mode?**
st_mode packs the file type (high bits) and the permission bits (low bits) into one integer. Bitwise AND with a mask isolates the bits we want. For the type, `(st_mode & S_IFMT) == S_IFDIR` isolates the type field and compares it with the directory constant; macros like S_ISDIR(mode) and S_ISREG(mode) do exactly this. For permissions, `st_mode & S_IRUSR` is non-zero only if the owner-read bit is set, and the same works for S_IWUSR, S_IXUSR, S_IRGRP, S_IROTH and so on. The special bits S_ISUID, S_ISGID and S_ISVTX are tested the same way and change the x position to s, S, t or T.
