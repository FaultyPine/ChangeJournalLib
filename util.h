#pragma once

#include <Windows.h>
#include <WinIoCtl.h>
#include <string_view>
#include <string>
#include "shared.h"


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

const char* usnReasonStrMap[] = 
{
   #define X(usnReason) "USN_REASON" #usnReason,
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

std::string UsnReasonToString(DWORD Reason)
{
   std::string reasonStr = {};
   for (int i = 0; i < NUM_USN_REASONS; i++)
   {
      uint32_t reason = UsnReasons[i];
      if (Reason & reason)
      {
         reasonStr += usnReasonStrMap[i];
         reasonStr += " | ";
      }
   }
   return reasonStr;
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
