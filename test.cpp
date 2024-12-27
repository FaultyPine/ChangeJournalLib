
#include <stdio.h>
#include "change_journal.h"

#define KILOBYTES_BYTES(kb) (kb*1024)
#define MEGABYTES_BYTES(mb) (mb*KILOBYTES_BYTES(1024))
#define GIGABYTES_BYTES(gb) (gb*MEGABYTES_BYTES(1024))

void PrintTimestamp(LARGE_INTEGER timestamp, const char* msg);

int main()
{
   // usn journals are typically 32mb
   Journal journal;
   if (!InitializeJournal('c', MEGABYTES_BYTES(4), journal))
   {
      printf("err\n");
   }
   printf("Journal size: %llu\n", journal.journalData.MaximumSize);
   printf("USN Range: [%lld, %lld]\n", journal.journalData.FirstUsn, journal.journalData.NextUsn); 

   // 1: read in manually, iterate manually
   constexpr size_t bufferSize = KILOBYTES_BYTES(50);
   char* buffer = new char[bufferSize];
   uint64_t bytesRead = 0;
   USN nextUsn = ReadJournal(journal, buffer, bufferSize, 0, &bytesRead);
   // buffer contains USN of last entry it read, then all the actual records
   PUSN_RECORD UsnRecords = (PUSN_RECORD)(buffer + sizeof(USN));
   // number of bytes remaining in the buffer of the collected USN_RECORDs
   size_t remainingBytesInBuffer = bytesRead - sizeof(USN);

   // Find the first record
   PUSN_RECORD UsnRecord = (PUSN_RECORD)(((PUCHAR)buffer) + sizeof(USN));  

   // This loop could go on for a while, depending on the current buffer size.
   while(remainingBytesInBuffer > 0)
   {
      constexpr size_t pathBufferSize = 200;
      wchar_t pathBuffer[pathBufferSize] = {0};
      if (GetPathFromRecord(journal.handle, UsnRecord, pathBuffer, pathBufferSize))
      {
         printf("USN Record: %c:%ls\n", journal.drive, pathBuffer);
         PrintTimestamp(UsnRecord->TimeStamp, "\t\tRecord timestamp: ");
      }
      remainingBytesInBuffer -= UsnRecord->RecordLength;
      // Find the next record
      UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + 
               UsnRecord->RecordLength); 
   }

   // 2. Read in automatically, using a callback to process each usn record
   /*
   if (!ReadJournal(journal, 10, [](const Journal& journal, PUSN_RECORD UsnRecord){
      constexpr size_t pathBufferSize = 200;
      wchar_t pathBuffer[pathBufferSize] = {0};
      if (GetPathFromRecord(journal.handle, UsnRecord, pathBuffer, pathBufferSize))
      {
         PrintTimestamp(UsnRecord->TimeStamp, "USN Record timestamp: ");
      }
   }, nextUsn))
   {
      printf("Did not fully complete ReadJournal\n");
   }
   */

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
