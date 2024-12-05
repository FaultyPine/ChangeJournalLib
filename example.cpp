#include <Windows.h>
#include <WinIoCtl.h>
#include <stdio.h>
#include <string_view>
#include "shared.h"
#define BUF_LEN 4096

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

struct FileInfo
{
   std::wstring_view pathBuffer = {};
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
      return false;
   }
   // this is without the volume name, so C: isn't present
   outFileInfo.pathBuffer = { file_name_info->FileName, file_name_info->FileNameLength / 2 }; // dunno why, but the length seems to be 2x what it actually is
   return true;
}

bool stringview_startswith(char* str, char* startswith, int strlen)
{

}

void example_main()
{
   HANDLE hVol;
   CHAR Buffer[BUF_LEN];

   USN_JOURNAL_DATA JournalData;
   // https://stackoverflow.com/questions/46978678/walking-the-ntfs-change-journal-on-windows-10
   // _V0 is the old version, but using _V1, which is supposed to be for win 10, seems to garble the data
   READ_USN_JOURNAL_DATA_V0 ReadData = {0, 0xFFFFFFFF, FALSE, 0, 0};
   PUSN_RECORD UsnRecord;  

   DWORD dwBytes;
   DWORD dwRetBytes;
   int I;

   // C:\Dev\usn_change_journal_playground\testdir
   hVol = CreateFile( TEXT("\\\\.\\c:"), 
               GENERIC_READ | GENERIC_WRITE, 
               FILE_SHARE_READ | FILE_SHARE_WRITE,
               NULL,
               OPEN_EXISTING,
               0,
               NULL);

   if( hVol == INVALID_HANDLE_VALUE )
   {
      printf("CreateFile failed (%d)\n", GetLastError());
      return;
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
      return;
   }

   ReadData.UsnJournalID = JournalData.UsnJournalID;
   // https://stackoverflow.com/questions/46978678/walking-the-ntfs-change-journal-on-windows-10
   // #if (NTDDI_VERSION >= NTDDI_WIN8)
   // ReadData.MinMajorVersion = JournalData.MinSupportedMajorVersion;
   // ReadData.MaxMajorVersion = JournalData.MaxSupportedMajorVersion;
   // #endif
   // -----------

   printf( "Journal ID: %I64x\n", JournalData.UsnJournalID );
   printf( "FirstUsn: %I64x\n\n", JournalData.FirstUsn );

   for(I=0; I<=10; I++)
   {
      memset( Buffer, 0, BUF_LEN );

      if( !DeviceIoControl( hVol, 
            FSCTL_READ_USN_JOURNAL, 
            &ReadData,
            sizeof(ReadData),
            &Buffer,
            BUF_LEN,
            &dwBytes,
            NULL) )
      {
         printf( "Read journal failed (%d)\n", GetLastError());
         return;
      }

      dwRetBytes = dwBytes - sizeof(USN);

      // Find the first record
      UsnRecord = (PUSN_RECORD)(((PUCHAR)Buffer) + sizeof(USN));  

      //printf( "****************************************\n");

      // This loop could go on for a long time, given the current buffer size.
      while( dwRetBytes > 0 )
      {

         FileInfo fileInfo = {};
         FileInfoFromRefNum(hVol, UsnRecord, fileInfo);
         //printf("File full path: %.*S\n", fileInfo.pathBuffer.size(), fileInfo.pathBuffer.data());

         //if (fileInfo.pathBuffer._Starts_with(L"/C:/Dev/usn_change_journal_playground"))
         if (UsnRecord->Reason & USN_REASON_FILE_CREATE)
         {
            printf( "USN: %I64x\n", UsnRecord->Usn );
            printf("File name: %.*S\n", 
                     UsnRecord->FileNameLength/2, 
                     UsnRecord->FileName );
            printf( "Reason: %x\n", UsnRecord->Reason );
            printf("File full path: %.*S\n", fileInfo.pathBuffer.size(), fileInfo.pathBuffer.data());
            printf( "\n" );
         }

         dwRetBytes -= UsnRecord->RecordLength;

         // Find the next record
         UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + 
                  UsnRecord->RecordLength); 
      }
      // Update starting USN for next call
      ReadData.StartUsn = *(USN *)&Buffer; 
   }

   CloseHandle(hVol);

}