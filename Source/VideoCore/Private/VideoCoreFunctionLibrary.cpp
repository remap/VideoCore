// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCoreFunctionLibrary.h"
#include "Containers/UnrealString.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "HAL/PlatformApplicationMisc.h"
#include "VideoCorePlane.h"

struct FPlaneProxyArchive : public FObjectAndNameAsStringProxyArchive
{
    FPlaneProxyArchive(FArchive& ar) : FObjectAndNameAsStringProxyArchive(ar, true)
    {
        ArIsSaveGame = true;
    }
};

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

TArray<FPlaneRecord> UVideoCoreFunctionLibrary::SerializePlanes(const TArray<AVideoCorePlane*>& planes)
{
    TArray<FPlaneRecord> records;

    for (auto p : planes)
    {
        FPlaneRecord rec;
        rec.className_ = p->GetClass()->GetPathName();
        rec.planeName_ = p->GetName();

        UE_LOG(LogTemp, Log, TEXT("serializing plane %s class %s"),
            *rec.planeName_, *rec.className_);
        
        FMemoryWriter memWriter(rec.planeData_);
        FPlaneProxyArchive planeArchive(memWriter);
        p->Serialize(planeArchive);

        UE_LOG(LogTemp, Log, TEXT("serialized plane %s into array %d bytes"),
            *rec.planeName_, rec.planeData_.Num());

        records.Add(rec);
    }

    return records;
}

void UVideoCoreFunctionLibrary::DeserializePlane(AVideoCorePlane* plane, const FPlaneRecord& rec)
{
    FMemoryReader memReader(rec.planeData_);
    FPlaneProxyArchive planeArchive(memReader);

    plane->Serialize(planeArchive);
    plane->OnDeserializationCompleted();
}

TMap<FString, FString> UVideoCoreFunctionLibrary::GetCommandLineArgs()
{
    TMap<FString, FString> args;

    {
        FString fileName;

        if (FParse::Value(FCommandLine::Get(), TEXT("videoCoreGame="), fileName)) {
            fileName = fileName.Replace(TEXT("="), TEXT("")).Replace(TEXT("\""), TEXT("")); // replace quotes
            args.Add(FString("videoCoreGame"), fileName);
        }
    }
    {
        FString instanceTag;
        if (FParse::Value(FCommandLine::Get(), TEXT("videoCoreInstanceTag="), instanceTag)) {
            instanceTag = instanceTag.Replace(TEXT("="), TEXT("")).Replace(TEXT("\""), TEXT("")); // replace quotes
            args.Add(FString("videoCoreInstanceTag"), instanceTag);
        }
    }
    

    return args;
}

FLinearColor UVideoCoreFunctionLibrary::getImagePixelValue(UImage* imgWidget, float u, float v)
{
    UObject* texResource = imgWidget->Brush.GetResourceObject();

    UTexture* tex = Cast<UTexture>(texResource);
    if (tex)
    {
        auto pd = tex->GetRunningPlatformData();
        //FColor *imageData = static_cast<FColor*>()
    }

    return FLinearColor();
}

void UVideoCoreFunctionLibrary::setCompMaterialParamScalar(FCompositingMaterial mat, FName paramName, float paramValue)
{
    UMaterialInstanceDynamic* matInstance = mat.GetMID();

    if (matInstance)
    {
        matInstance->SetScalarParameterValue(paramName, paramValue);
    }
}

void UVideoCoreFunctionLibrary::setCompMaterialParamVector(FCompositingMaterial mat, FName paramName, FLinearColor color)
{
    mat.SetVectorOverride(paramName, color);
}

void UVideoCoreFunctionLibrary::setCompMaterialParamTexture(FCompositingMaterial mat, FName paramName, UTexture* texture)
{
    mat.SetMaterialParam(paramName, texture);
}

float UVideoCoreFunctionLibrary::getCompMaterialParamScalar(FCompositingMaterial mat, FName paramName)
{
    float p = 0;

    FHashedMaterialParameterInfo i;
    if (mat.GetMID())
     mat.GetMID()->GetScalarParameterValue(FHashedMaterialParameterInfo(paramName), p);
    return p;
}

float UVideoCoreFunctionLibrary::getCompMaterialParamScalarDefault(FCompositingMaterial mat, FName paramName)
{
    float p = 0;
    if (mat.GetMID())
     mat.GetMID()->GetScalarParameterDefaultValue(FHashedMaterialParameterInfo(paramName), p);
    return p;
}

FLinearColor UVideoCoreFunctionLibrary::getCompMaterialParamVector(FCompositingMaterial mat, FName paramName)
{
    FLinearColor c;
    if (mat.GetVectorOverride(paramName, c))
        return c;

    if (mat.GetMID())
      mat.GetMID()->GetLinearColorParameterValue(FHashedMaterialParameterInfo(paramName), c);
    return c;
}