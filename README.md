https://learn.microsoft.com/en-us/windows/win32/fileio/change-journals


https://learn.microsoft.com/en-us/windows/win32/fileio/change-journal-operations


https://learn.microsoft.com/en-us/windows/win32/fileio/walking-a-buffer-of-change-journal-records


https://github.com/dfs-minded/indexer-plus-plus/blob/master/NTFSReader/NTFSChangesWatcher.cpp
https://github.com/osquery/osquery/blob/master/osquery/events/windows/usn_journal_reader.h


// using FILE_TRAVERSE, and FSCTL_READ_UNPRIVILEGED_USN_JOURNAL are required to get this working without
admin perms. This behavior is totally undocumented lol. 

There's some fiddling with StartUSN and NextUSN that indicate the queried "state" of the journal.
I.E. you can query the journal with X usn, and it'll give you the updates *since then*. 