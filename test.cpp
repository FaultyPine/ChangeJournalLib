
#include <stdio.h>
#include "change_journal.h"

#include <pathcch.h>
#pragma comment(lib, "Pathcch.lib")

#define KILOBYTES_BYTES(kb) (kb*1024)
#define MEGABYTES_BYTES(mb) (mb*KILOBYTES_BYTES(1024))
#define GIGABYTES_BYTES(gb) (gb*MEGABYTES_BYTES(1024))
void PrintTimestamp(LARGE_INTEGER timestamp, const char* msg);

int main(int argc, char** argv)
{
   // usn journals are typically 32mb
   Journal journal;
   if (!InitializeJournal('c', journal))
   {
      printf("err\n");
   }
   printf("Journal size: %llu\n", journal.journalData.MaximumSize);
   printf("USN Range: [%lld, %lld]\n", journal.journalData.FirstUsn, journal.journalData.NextUsn); 

   constexpr size_t bufferSize = MEGABYTES_BYTES(1);
   char* buffer = new char[bufferSize];


   // 1: read in manually, iterate manually
   uint64_t bytesRead = 0;
   USN nextUsn = ReadJournal(journal, buffer, bufferSize, 0, &bytesRead);
   // buffer contains USN of last entry it read, then all the actual records
   PUSN_RECORD UsnRecords = (PUSN_RECORD)(buffer + sizeof(USN));
   // number of bytes remaining in the buffer of the collected USN_RECORDs
   size_t remainingBytesInBuffer = bytesRead - sizeof(USN);

   // Find the first record
   PUSN_RECORD UsnRecord = (PUSN_RECORD)(((PUCHAR)buffer) + sizeof(USN));  

   constexpr uint32_t TMP_PATH_BUFFER_SIZE = 200;
   // This loop could go on for a while, depending on the current buffer size.
   while(remainingBytesInBuffer > 0)
   {
      wchar_t pathBuffer[TMP_PATH_BUFFER_SIZE] = {0};
      if (GetPathFromRecord(journal, UsnRecord, pathBuffer, TMP_PATH_BUFFER_SIZE))
      {
         //printf("USN Record: %c:%ls\n", journal.drive, pathBuffer);
         //PrintTimestamp(UsnRecord->TimeStamp, "\t\tRecord timestamp: ");
      }
      remainingBytesInBuffer -= UsnRecord->RecordLength;
      // Find the next record
      UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + 
               UsnRecord->RecordLength); 
   }



   // 2. Read in automatically, using a callback to process each usn record
   constexpr uint32_t NUM_READ_ATTEMPTS = 20;

   wchar_t exeFolder[MAX_PATH];
   GetModuleFileNameW(NULL, exeFolder, MAX_PATH);
   PathCchRemoveFileSpec(exeFolder, MAX_PATH);
   printf("Reading in remaining entries using callback method...\n Looking only for .cpp changes in %ls \n", exeFolder);

   if (!ReadJournal(journal, buffer, bufferSize, NUM_READ_ATTEMPTS, [](const Journal& journal, PUSN_RECORD UsnRecord, void* userData)
   {

      wchar_t pathBuffer[TMP_PATH_BUFFER_SIZE] = {0};
      if (GetPathFromRecord(journal, UsnRecord, pathBuffer, TMP_PATH_BUFFER_SIZE))
      {
         if (wcsstr(pathBuffer, (wchar_t*)userData) && wcsstr(pathBuffer, L".cpp"))
         {
            printf("USN Record: %ls\n", pathBuffer);
            PrintTimestamp(UsnRecord->TimeStamp, "USN Record timestamp: ");
         }
      }
   }, &exeFolder[0], nextUsn))
   {
      //printf("Did not fully complete ReadJournal\n");
   }
   

   DeinitializeJournal(journal);
   return 0;
}




#define ISO_DATETIME_LEN 26
#include <tchar.h>
#include <strsafe.h>
PTCHAR TimeStampToIso8601(
	LARGE_INTEGER* timeStamp,
	PTCHAR pszBuffer
)
{
	SYSTEMTIME systemTime;
	if (pszBuffer == NULL)
	{
		return NULL;
	}
	memset(pszBuffer, 0, ISO_DATETIME_LEN);
	if (FileTimeToSystemTime((PFILETIME)timeStamp, &systemTime))
	{
		StringCchPrintf(
			pszBuffer,
			ISO_DATETIME_LEN,
			TEXT("%04i-%02i-%02iT%02i:%02i:%02i.%03iZ"),
			systemTime.wYear,
			systemTime.wMonth,
			systemTime.wDay,
			systemTime.wHour,
			systemTime.wMinute,
			systemTime.wSecond,
			systemTime.wMilliseconds
		);
	}
	return pszBuffer;
}

void PrintTimestamp(LARGE_INTEGER timestamp, const char* msg)
{
   TCHAR strIsoDateTime[ISO_DATETIME_LEN];
   PTCHAR oldestJournalEntry = TimeStampToIso8601(&timestamp, strIsoDateTime);
   printf("%s %s  %lld\n", msg, strIsoDateTime, timestamp.QuadPart);
}
