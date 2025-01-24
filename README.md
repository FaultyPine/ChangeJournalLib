# Change Journals

The purpose of a change journal is to speed up operations that might conventionally be implemented with a recursive loop over all files in a directory
comparing timestamps and potentially content hashes to determine which files have changed.
A change journal stores filesystem events at the moment of the file being accessed, effectively removing the need to scan for changes like before.

A Change Journal is a (currently only) windows concept that boils down to storing a list of recently modified/deleted/accessed/etc files.
Each time a file is (for example) modified, a record of that "change" is recorded in the OS internal "journal". This entry contains information
like the reason of the change (overwrite, truncate, close, open, permission/attribute modification, etc), a timestamp of the change, and the
USN (update sequence number) of the change. The USN is incremented for every file access/modification.

# Usage
Include change_journal.h and compile change_journal.cpp with your build system.

# Building

make sure clang is in your env PATH variable, then run build.bat.
Clang download: https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/clang+llvm-19.1.0-x86_64-pc-windows-msvc.tar.xz

# Example Usage:
See test.cpp

### References:
https://learn.microsoft.com/en-us/windows/win32/fileio/change-journals
https://learn.microsoft.com/en-us/windows/win32/fileio/change-journal-operations
https://learn.microsoft.com/en-us/windows/win32/fileio/walking-a-buffer-of-change-journal-records
https://github.com/dfs-minded/indexer-plus-plus/blob/master/NTFSReader/NTFSChangesWatcher.cpp
https://github.com/osquery/osquery/blob/master/osquery/events/windows/usn_journal_reader.h

Note: using FILE_TRAVERSE, and FSCTL_READ_UNPRIVILEGED_USN_JOURNAL are required to get this working without
admin perms. This behavior is completely undocumented...
