#include <stdio.h>
#include <array>
#include <windows.h>
#include "shared.h"
#include "example.h"



int64 GetUSN(const HANDLE& inFileHandle)
{
	PathBufferUTF16 buffer;
	DWORD available_bytes = 0;
	if (!DeviceIoControl(inFileHandle, FSCTL_READ_FILE_USN_DATA, nullptr, 0, buffer.data(), buffer.size() * sizeof(buffer[0]), &available_bytes, nullptr))
	{
    	printf("Failed to get USN data"); // TODO add file path to message
        return 0;
    }

	USN_RECORD_COMMON_HEADER* record_header = (USN_RECORD_COMMON_HEADER*)buffer.data();
	if (record_header->MajorVersion == 2)
	{
		USN_RECORD_V2* record = (USN_RECORD_V2*)buffer.data();
		return record->Usn;	
	}
	else if (record_header->MajorVersion == 3)
	{
		USN_RECORD_V3* record = (USN_RECORD_V3*)buffer.data();
		return record->Usn;
	}
	else
	{
		printf("Got unexpected USN record version (%i.%i)", record_header->MajorVersion, record_header->MinorVersion);
		return 0;
	}
}


int main()
{
    const char* monitorDir = "testdir";
    example_main();
    return 0;
}