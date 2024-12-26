#ifndef _CHANGE_JOURNAL_LIB_H
#define _CHANGE_JOURNAL_LIB_H

#include <cstdint>
#include <Windows.h>
#include <WinIoCtl.h>

struct Journal
{
    char drive;
    char* buffer;
    uint64_t bufferSize;
    // includes the size (bytes) of the full journal's internal OS buffer
    // the Oldest and Latest USN, and the ID
    USN_JOURNAL_DATA journalData;
};


bool InitializeJournal(char drive, uint64_t bufferSize, Journal& journal);
bool DeinitializeJournal(const Journal& journal);


typedef void(*UsnRecordCallback)(const Journal& journal, PUSN_RECORD record);
// reads journal entries until either we've read all the entries, or numReadAttempts is reached
// calling cb on each record
bool ReadJournal(const Journal& journal, uint32_t numReadAttempts, UsnRecordCallback cb, uint64_t startingUSN = 0);
// read journal entries into the given buffer. Either all entries fit in the buffer, 
// or there will be some entries that aren't read
// returns the USN of the last entry that was read into buffer. Use this as the starting usn
// of subsequent reads to continue reading from where this call left off
// a starting USN of 0 means "read from the very start of the journal"
USN ReadJournal(const Journal& journal, void* buffer, size_t bufferSize, uint64_t startingUSN = 0, uint64_t* bytesRead = nullptr);

// convinience method - checks for interesting usn reasons, and some edge cases
bool IsInterestingEntry(PUSN_RECORD UsnRecord);

constexpr size_t REASONSTR_MAX_LENGTH = 100;
void UsnReasonToString(DWORD Reason, char (&reasonStr)[REASONSTR_MAX_LENGTH]);

bool GetPathFromRecord(HANDLE volume, PUSN_RECORD usnRecord, wchar_t* pathBuffer, size_t pathBufferSize);

#endif