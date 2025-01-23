#include "change_journal.h"

#include <array>
#include <string>

#include <stdio.h>

constexpr size_t cWin32MaxPathSizeUTF16 = 32768;
constexpr size_t cWin32MaxPathSizeUTF8  = 32768 * 3ull;	// UTF8 can use up to 6 bytes per character, but let's suppose 3 is good enough on average.

using PathBufferUTF16 = std::array<wchar_t, cWin32MaxPathSizeUTF16>;
using PathBufferUTF8  = std::array<char, cWin32MaxPathSizeUTF8>;


#define USN_REASONS_ENUM \
X(DATA_OVERWRITE)\
X(DATA_EXTEND)\
X(DATA_TRUNCATION)\
X(NAMED_DATA_OVERWRITE)\
X(NAMED_DATA_EXTEND)\
X(NAMED_DATA_TRUNCATION)\
X(FILE_CREATE)\
X(FILE_DELETE)\
X(EA_CHANGE)\
X(SECURITY_CHANGE)\
X(RENAME_OLD_NAME)\
X(RENAME_NEW_NAME)\
X(INDEXABLE_CHANGE)\
X(BASIC_INFO_CHANGE)\
X(HARD_LINK_CHANGE)\
X(COMPRESSION_CHANGE)\
X(ENCRYPTION_CHANGE)\
X(OBJECT_ID_CHANGE)\
X(REPARSE_POINT_CHANGE)\
X(STREAM_CHANGE)\
X(TRANSACTED_CHANGE)\
X(INTEGRITY_CHANGE)\
X(DESIRED_STORAGE_CLASS_CHANGE)\
X(CLOSE)\

struct SizedString
{
    char* data;
    size_t size;
};

SizedString usnReasonStrMap[] = 
{
   #define X(usnReason) {"USN_REASON" #usnReason, sizeof("USN_REASON" #usnReason)},
   USN_REASONS_ENUM
   #undef X
};
uint32_t UsnReasons[] = 
{
   #define X(usnReason) USN_REASON_##usnReason,
   USN_REASONS_ENUM
   #undef X
};
constexpr uint32_t NUM_USN_REASONS = sizeof(UsnReasons)/sizeof(UsnReasons[0]);
static_assert(NUM_USN_REASONS == sizeof(usnReasonStrMap)/sizeof(usnReasonStrMap[0]));

void UsnReasonToString(DWORD Reason, char (&reasonStr)[REASONSTR_MAX_LENGTH])
{
    int strIdx = 0;
    for (int i = 0; i < NUM_USN_REASONS; i++)
    {
        uint32_t reason = UsnReasons[i];
        if (Reason & reason)
        {
            size_t reasonStrSize = usnReasonStrMap->size;
            memcpy(reasonStr + strIdx, usnReasonStrMap[i].data, reasonStrSize);
            strIdx += reasonStrSize;
            _ASSERT(strIdx < REASONSTR_MAX_LENGTH);
            char seperator[] = " | ";
            memcpy(reasonStr + strIdx, seperator, sizeof(seperator));
            strIdx += sizeof(seperator);
            _ASSERT(strIdx < REASONSTR_MAX_LENGTH);
        }
    }
}

// for USN_RECORD_V2
FILE_ID_DESCRIPTOR getFileIdDescriptor(const DWORDLONG fileId)
{
   FILE_ID_DESCRIPTOR fileDescriptor;
   fileDescriptor.Type = FileIdType;
   fileDescriptor.FileId.QuadPart = fileId;
   fileDescriptor.dwSize = sizeof(fileDescriptor);
   return fileDescriptor;
}

// for USN_RECORD_V3
FILE_ID_DESCRIPTOR getFileIdDescriptor(const FILE_ID_128& fileId)
{
   FILE_ID_DESCRIPTOR fileDescriptor;
   fileDescriptor.Type = ExtendedFileIdType;
   fileDescriptor.ExtendedFileId = fileId;
   fileDescriptor.dwSize = sizeof(fileDescriptor);
   return fileDescriptor;
}

// currently only getting file path, could get more stuff, anything from _FILE_INFO_BY_HANDLE_CLASS (altho each info type would mean another kernel call)
// https://stackoverflow.com/questions/31763195/how-to-get-the-full-path-for-usn-journal-query
bool GetPathFromRecord(const Journal& journal, PUSN_RECORD usnRecord, wchar_t* pathBuffer, size_t pathBufferSize)
{
    FILE_ID_DESCRIPTOR fileDesc = getFileIdDescriptor(usnRecord->FileReferenceNumber);
    HANDLE file = OpenFileById(journal.handle, &fileDesc, 0, 0, 0, 0);
    // Get the full path as utf16.
    // PFILE_NAME_INFO contains the filename without the drive letter and column in front (ie. without the C:).
    PathBufferUTF16 wpath_buffer;
    PFILE_NAME_INFO file_name_info = (PFILE_NAME_INFO)wpath_buffer.data();
    if (!GetFileInformationByHandleEx(file, FileNameInfo, file_name_info, wpath_buffer.size() * sizeof(wpath_buffer[0])))
    {
        //printf("Failed to get file information!\n");
        return false;
    }
    // this is without the volume name, so C: isn't present
    if (file_name_info->FileNameLength > pathBufferSize)
    {
        //printf("we've got a problem");
        return false;
    }
    _ASSERT(file_name_info->FileNameLength < pathBufferSize+2);
    mbstowcs(&pathBuffer[0], &journal.drive, 1); // Copy drive name
    memcpy(&pathBuffer[1], L":", sizeof(pathBuffer[1])); // copy drive identifier
    memcpy(pathBuffer+2, file_name_info->FileName, file_name_info->FileNameLength);
    return true;
}


bool InitializeJournal(char drive, Journal& journal)
{
    drive = toupper(drive);
    //journal.buffer = new char[bufferSize];
    //journal.bufferSize = bufferSize;
    journal.drive = drive;

    constexpr const char* DRIVE_PATH_TEMPLATE = "\\\\.\\C:";
    char drivePath[sizeof(DRIVE_PATH_TEMPLATE)];
    memcpy(drivePath, DRIVE_PATH_TEMPLATE, sizeof(DRIVE_PATH_TEMPLATE));
    drivePath[4] = drive;
    journal.handle = CreateFile((LPCSTR)drivePath, 
                (DWORD)FILE_TRAVERSE, // required to work without admin perms
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
    if (journal.handle == INVALID_HANDLE_VALUE)
    {
        // TOOD: proper error enum
        return false;
    }

    DWORD bytesRead;
    if(!DeviceIoControl(journal.handle, 
            FSCTL_QUERY_USN_JOURNAL, 
            NULL,
            0,
            &journal.journalData,
            sizeof(journal.journalData),
            &bytesRead,
            NULL) )
    {
        return false;
    }
    return true;
}

bool DeinitializeJournal(const Journal& journal)
{
    CloseHandle(journal.handle);
    //delete(journal.buffer);
}

bool IsInterestingEntry(PUSN_RECORD UsnRecord)
{
    constexpr uint32_t cInterestingReasons = USN_REASON_FILE_CREATE |     // File was created.
                                        USN_REASON_FILE_DELETE |     // File was deleted.
                                        USN_REASON_DATA_OVERWRITE |  // File was modified.
                                        USN_REASON_DATA_EXTEND |     // File was modified.
                                        USN_REASON_DATA_TRUNCATION | // File was modified.
                                        USN_REASON_RENAME_NEW_NAME |  // File was renamed or moved (possibly to the recyle bin). That's essentially a delete and a create.
                                        USN_REASON_CLOSE // file was closed
                                        ;
    // file was created and then removed super quick
    bool createdAndDeletedQuickly = (UsnRecord->Reason & (USN_REASON_FILE_DELETE | USN_REASON_FILE_CREATE)) == (USN_REASON_FILE_DELETE | USN_REASON_FILE_CREATE);
    return (UsnRecord->Reason & cInterestingReasons && !createdAndDeletedQuickly);
}

USN ReadJournal(const Journal& journal, void* buffer, size_t bufferSize, uint64_t startingUSN, uint64_t* bytesRead)
{
    READ_USN_JOURNAL_DATA_V0 ReadData = {0};
    ReadData.ReasonMask = ~0;
    ReadData.StartUsn = startingUSN ? startingUSN : journal.journalData.FirstUsn;
    ReadData.UsnJournalID = journal.journalData.UsnJournalID;
    //PUSN_RECORD UsnRecord;
    uint64_t unusedBytesRead;
    bool bStatus = DeviceIoControl(journal.handle, 
            FSCTL_READ_UNPRIVILEGED_USN_JOURNAL, 
            &ReadData,
            sizeof(ReadData),
            buffer,
            bufferSize,
            bytesRead ? (DWORD*)bytesRead : (DWORD*)unusedBytesRead,
            NULL);
    if (!bStatus) 
    {
        // this is fine - ran out of journal data to read
        if ((ERROR_HANDLE_EOF == GetLastError()) || (ERROR_WRITE_PROTECT == GetLastError()))
        {
            printf("Reached end of journal!\n");
        }
        printf( "Read journal (manual) failed (%d)\n", GetLastError());
        return 1;
    }
    //UsnRecord = (PUSN_RECORD)(((PUCHAR)buffer) + sizeof(USN));  
    USN usn = *(USN *)buffer; 
    return usn;
}

bool ReadJournal(
    const Journal& journal, 
    void* buffer,
    size_t bufferSize,
    uint32_t numReadAttempts, 
    UsnRecordCallback cb, 
    void* userData,
    uint64_t startingUSN)
{
    // https://stackoverflow.com/questions/46978678/walking-the-ntfs-change-journal-on-windows-10
    // _V0 is the old version, but using _V1, which is supposed to be for win 10, seems to garble the data
    READ_USN_JOURNAL_DATA_V0 ReadData = {0};
    ReadData.ReasonMask = ~0;
    ReadData.StartUsn = startingUSN;
    ReadData.StartUsn = startingUSN ? startingUSN : journal.journalData.FirstUsn;
    ReadData.UsnJournalID = journal.journalData.UsnJournalID;
    PUSN_RECORD UsnRecord;  
    DWORD bytesRead = 0;
    DWORD remainingBufferBytes = 0;
    int i = 0;
    for(; i < numReadAttempts; i++)
    {
        memset(buffer, 0, bufferSize);

        bool bStatus = DeviceIoControl(journal.handle, 
            FSCTL_READ_UNPRIVILEGED_USN_JOURNAL, 
            &ReadData,
            sizeof(ReadData),
            buffer,
            bufferSize,
            &bytesRead,
            NULL);
        if (!bStatus) 
        {
            // this is fine - ran out of journal data to read
            if ((ERROR_HANDLE_EOF == GetLastError()) || (ERROR_WRITE_PROTECT == GetLastError()))
            {
                printf("Reached end of journal!\n");
                break;
            }
            printf( "Read journal (callback) failed (%d)\n", GetLastError());
            return true;
        }

        remainingBufferBytes = bytesRead - sizeof(USN);

        // Find the first record
        UsnRecord = (PUSN_RECORD)(((PUCHAR)buffer) + sizeof(USN));  

        // This loop could go on for a while, depending on the current buffer size.
        while(remainingBufferBytes > 0)
        {
            //liMinDate.QuadPart = min(liMinDate.QuadPart, UsnRecord->TimeStamp.QuadPart);
            cb(journal, UsnRecord, userData);
            remainingBufferBytes -= UsnRecord->RecordLength;
            // Find the next record
            UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + 
                    UsnRecord->RecordLength); 
        }
        // Update starting USN for next call
        ReadData.StartUsn = *(USN *)buffer; 
    }

    if (i == numReadAttempts)
    {
        printf("Exhausted max number of journal read attempts\n");
        return false;
    }
    return true;
}