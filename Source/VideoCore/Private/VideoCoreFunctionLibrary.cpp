// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCoreFunctionLibrary.h"
#include "Containers/UnrealString.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "HAL/PlatformApplicationMisc.h"

TArray<uint8> UVideoCoreFunctionLibrary::ToBytesArray(const FPlaneData& d)
{
    FPlaneData pData(d);
	FBufferArchive binArchive;

    binArchive << pData.transform_;
    binArchive << pData.defaultScale_;
    binArchive << pData.autorotateLocalRotation_;
    binArchive << pData.isAutoRotate_;
    binArchive << pData.textureCrop_;

    return TArray<uint8>(binArchive);
}

FPlaneData UVideoCoreFunctionLibrary::FromBytesArray(const TArray<uint8>& d, int &offset)
{
    FPlaneData pData;
    FMemoryReader binArchive = FMemoryReader(d, true);
    binArchive.Seek(0);

    binArchive << pData.transform_;
    binArchive << pData.defaultScale_;
    binArchive << pData.autorotateLocalRotation_;
    binArchive << pData.isAutoRotate_;
    binArchive << pData.textureCrop_;

    offset = binArchive.Tell();

    return pData;
}

TArray<uint8> UVideoCoreFunctionLibrary::StringToByteArray(const FString& str)
{
	return TArray<uint8>((uint8*)TCHAR_TO_ANSI(*str), str.Len());

}

FString UVideoCoreFunctionLibrary::ByteArrayToString(const TArray<uint8>& barr)
{
	return FString(ANSI_TO_TCHAR((ANSICHAR*)barr.GetData()));
}


void UVideoCoreFunctionLibrary::ClipboardCopy(const FString& str)
{
    FPlatformApplicationMisc::ClipboardCopy(*str);
}