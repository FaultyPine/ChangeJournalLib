
#include <stdio.h>


#include "shared.h"
#include "util.h"

// TODO: would be cool to use the First and Next USN numbers
// along with their timestamps to quickly get the timestamp range of the journal

#define KILOBYTES_BYTES(kb) (kb*1024)
#define MEGABYTES_BYTES(mb) (mb*KILOBYTES_BYTES(1024))
#define GIGABYTES_BYTES(gb) (gb*MEGABYTES_BYTES(1024))

// usn journals are typically 32mb
#define BUF_LEN MEGABYTES_BYTES(4)

LARGE_INTEGER liMinDate;

struct FileInfo
{
   std::string pathBuffer = {};
};

// currently only getting file path, could get more stuff, anything from _FILE_INFO_BY_HANDLE_CLASS (altho each info type would mean another kernel call)
// https://stackoverflow.com/questions/31763195/how-to-get-the-full-path-for-usn-journal-query
bool FileInfoFromRefNum(HANDLE volume, PUSN_RECORD usnRecord, FileInfo& outFileInfo)
{
   FILE_ID_DESCRIPTOR fileDesc = getFileIdDescriptor(usnRecord->FileReferenceNumber);
   HANDLE file = OpenFileById(volume, &fileDesc, 0, 0, 0, 0);
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
   // NOTE: FileName is WCHAR, TODO: handle that properly
   outFileInfo.pathBuffer.assign((char*)file_name_info->FileName, file_name_info->FileNameLength); 
   return true;
}

void PrintTimestamp(LARGE_INTEGER timestamp, const char* msg)
{
   TCHAR strIsoDateTime[ISO_DATETIME_LEN];
   PTCHAR oldestJournalEntry = TimeStampToIso8601(&timestamp, strIsoDateTime);
   printf("%s %s  %lld\n", msg, strIsoDateTime, timestamp.QuadPart);
}

int main()
{
   HANDLE hVol;
   CHAR* Buffer = new CHAR[BUF_LEN];

   USN_JOURNAL_DATA JournalData;
   // https://stackoverflow.com/questions/46978678/walking-the-ntfs-change-journal-on-windows-10
   // _V0 is the old version, but using _V1, which is supposed to be for win 10, seems to garble the data
   READ_USN_JOURNAL_DATA_V0 ReadData = {0};
   ReadData.ReasonMask = ~0;
   PUSN_RECORD UsnRecord;  

   DWORD dwBytes;
   DWORD dwRetBytes;
   int I;
   liMinDate.QuadPart = MAXLONGLONG;

   // C:\Dev\usn_change_journal_playground\testdir
   hVol = CreateFile( TEXT("\\\\.\\c:"), 
               (DWORD)FILE_TRAVERSE, 
               FILE_SHARE_READ | FILE_SHARE_WRITE,
               nullptr,
               OPEN_EXISTING,
               FILE_ATTRIBUTE_NORMAL,
               nullptr);

   if( hVol == INVALID_HANDLE_VALUE )
   {
      printf("CreateFile failed (%d)\n", GetLastError());
      return 1;
   }

   if( !DeviceIoControl( hVol, 
          FSCTL_QUERY_USN_JOURNAL, 
          NULL,
          0,
          &JournalData,
          sizeof(JournalData),
          &dwBytes,
          NULL) )
   {
      printf( "Query journal failed (%d)\n", GetLastError());
      return 1;
   }

   ReadData.UsnJournalID = JournalData.UsnJournalID;
   ReadData.StartUsn = JournalData.FirstUsn;
   //ReadData.ReturnOnlyOnClose = false;
   //ReadData.ReasonMask = ~0;
   // https://stackoverflow.com/questions/46978678/walking-the-ntfs-change-journal-on-windows-10
   // #if (NTDDI_VERSION >= NTDDI_WIN8)
   // ReadData.MinMajorVersion = JournalData.MinSupportedMajorVersion;
   // ReadData.MaxMajorVersion = JournalData.MaxSupportedMajorVersion;
   // #endif
   // -----------

   printf( "Journal ID: %I64x\n", JournalData.UsnJournalID );
   printf( "FirstUsn: %I64x\n\n", JournalData.FirstUsn );

   int i = 0;
   constexpr uint32_t MAX_NUM_READ_ATTEMPTS = 10;
   for(; i < MAX_NUM_READ_ATTEMPTS; i++)
   {
      memset( Buffer, 0, BUF_LEN );

      bool bStatus = DeviceIoControl( hVol, 
            FSCTL_READ_UNPRIVILEGED_USN_JOURNAL, 
            &ReadData,
            sizeof(ReadData),
            Buffer,
            BUF_LEN,
            &dwBytes,
            NULL);
      if (!bStatus) 
      {
         // this is fine - ran out of journal data to read
         if ((ERROR_HANDLE_EOF == GetLastError()) || (ERROR_WRITE_PROTECT == GetLastError()))
			{
            printf("Reached end of journal!\n");
			   break;
			}
         printf( "Read journal failed (%d)\n", GetLastError());
         return 1;
      }

      dwRetBytes = dwBytes - sizeof(USN);

      // Find the first record
      UsnRecord = (PUSN_RECORD)(((PUCHAR)Buffer) + sizeof(USN));  

      //printf( "****************************************\n");

      // This loop could go on for a long time, given the current buffer size.
      while( dwRetBytes > 0 )
      {

         FileInfo fileInfo = {};
         if (!FileInfoFromRefNum(hVol, UsnRecord, fileInfo))
         {
            goto next_record;
         }

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
         bool isInRelevantFolder = fileInfo.pathBuffer.find("usn_change_journal_playground") != std::string::npos;
         if (UsnRecord->Reason & cInterestingReasons && !createdAndDeletedQuickly)
         {
            printf("is in current folder %i\n", isInRelevantFolder); // something about the fileInfo.pathBuffer is messed up
            printf( "USN: %I64x\n", UsnRecord->Usn );
            //printf("File name: %.*S\n", 
            //         UsnRecord->FileNameLength/2, 
            //         UsnRecord->FileName );
            //printf( "Reason: %x\n", UsnRecord->Reason );
            std::string usnReasonStr = UsnReasonToString(UsnRecord->Reason);
            printf( "Reason: %s\n", usnReasonStr.c_str());
            printf("File full path: %.*S\n", fileInfo.pathBuffer.size(), fileInfo.pathBuffer.data());
            PrintTimestamp(UsnRecord->TimeStamp, "Timestamp: ");
            printf( "\n" );
         }

         liMinDate.QuadPart = min(liMinDate.QuadPart, UsnRecord->TimeStamp.QuadPart);

         next_record:
         dwRetBytes -= UsnRecord->RecordLength;
         // Find the next record
         UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + 
                  UsnRecord->RecordLength); 
      }
      // Update starting USN for next call
      ReadData.StartUsn = *(USN *)Buffer; 
   }

   if (i == MAX_NUM_READ_ATTEMPTS)
   {
      printf("Exhausted max number of journal read attempts\n");
   }

   PrintTimestamp(liMinDate, "Oldest timestamp: ");

   CloseHandle(hVol);
   return 0;
}