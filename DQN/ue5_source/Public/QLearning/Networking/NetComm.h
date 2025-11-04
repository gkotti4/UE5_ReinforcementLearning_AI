#pragma once

#include "CoreMinimal.h"

class FSocket;

class UDEMYACTIONRPG_API NetComm
{
public:
	NetComm();
	~NetComm();

	bool bConnected = false;
	
	bool Init(const FString& Host = TEXT("127.0.0.1"), int32 Port = 5555);
	void Close();
	
	bool SendJson(const FString& Json);				// length-prefix helper (4)
	bool RecvJson(FString& OutJson);
	bool RecvUInt32(uint32& OutVal);				// for action id

	bool SendRaw(const uint8* Data, int32 Bytes);
	bool RecvRaw(uint8* Dest, int32 Bytes);

private:
	FSocket* Socket = nullptr;

	
};


/*
	Len= the total length of the JSON payload, expressed in host endianness (little‑endian on Windows/macOS/Linux x86).
	NetLen= that same length after htonl() (“host‑to‑network long”), i.e. big‑endian, which is the standard framing for TCP protocols.
	
 */