
#include "QLearning/Networking/NetComm.h"

#include "Sockets.h"
#include "SocketSubsystem.h"

#include "Windows/AllowWindowsPlatformTypes.h"	// or #undef SetPort
THIRD_PARTY_INCLUDES_START						// optional
	#include <winsock2.h>						// brings htonl/ntohl, also SetPortW macro
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"	// pops macros back off the stack




NetComm::NetComm()
{
}

NetComm::~NetComm()
{
	Close();
}

bool NetComm::Init(const FString& Host, int32 Port)
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	Socket = SS->CreateSocket(NAME_Stream, TEXT("DQNComm"), false);

	TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
	bool bOk;
	Addr->SetIp(*Host, bOk);
	Addr->SetPort(Port);
	if (!bOk || !Socket->Connect(*Addr)) { Close(); bConnected=false; return false; }

	bConnected = true;
	return true;
}

void NetComm::Close()
{
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
		bConnected = false;
	}
}

bool NetComm::SendJson(const FString& Json)
{
	FTCHARToUTF8 Msg(*Json);
	uint32 PayloadLen = Msg.Length();
	uint32 NetLenBE = htonl(PayloadLen);
	return SendRaw(reinterpret_cast<uint8*>(&NetLenBE), sizeof(uint32)) &&
		SendRaw((uint8*)Msg.Get(), PayloadLen);
}

bool NetComm::RecvJson(FString& OutJson)
{
	if (!Socket) return false;

	uint32 NetLenBE = 0;
	if (!RecvRaw((uint8*)&NetLenBE, 4)) return false;
	uint32 PayloadLen = ntohl(NetLenBE);

	TArray<uint8> Buffer; Buffer.SetNumUninitialized(PayloadLen);
	if (!RecvRaw(Buffer.GetData(), PayloadLen)) return false;

	OutJson = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));
	return true;
}

bool NetComm::RecvUInt32(uint32& OutVal)
{
	uint32 Net = 0;
	if (!RecvRaw((uint8*)&Net, sizeof(uint32))) return false;
	OutVal = ntohl(Net);
	
	UE_LOG(LogTemp, Display, TEXT("RecvUInt32: Received action %u"), OutVal);
	
	return true;
}

bool NetComm::SendRaw(const uint8* Data, int32 Bytes)
{
	int32 Sent = 0;
	return Socket && Socket->Send(Data, Bytes, Sent) && Sent == Bytes;
}

bool NetComm::RecvRaw(uint8* Dest, int32 Bytes)
{
	int32 Read = 0, Total = 0;
	while (Total < Bytes)
	{
		if (!Socket->Recv(Dest + Total, Bytes - Total, Read) || Read <= 0)
			return false;
		Total += Read;
	}
	return true;
}
