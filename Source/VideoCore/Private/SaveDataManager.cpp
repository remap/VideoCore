// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveDataManager.h"
#define SAVEDATAFILENAME "/Users/Remap/Desktop/sequence/"
// Sets default values
ASaveDataManager::ASaveDataManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASaveDataManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASaveDataManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool ASaveDataManager::SaveData()
{
//    SaveDir = SaveDir.TrimStartAndEnd();
//    if (!SaveDir.EndsWith(FString("/")) or !SaveDir.EndsWith(FString("\\")))
//    {
//        SaveDir = SaveDir.Append(FString("/"));
//    }
//    FString FilePath = SaveDir  + FString("Python/RecordedTransforms.txt");
//    FString FileContent = TEXT("");
//    for (auto Item : recordedTransforms)
//    {
//        FileContent += Item.ToString() + TEXT("\n");
//    }
//    FFileHelper::SaveStringToFile(FileContent, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
//    //now timesteps
////    FilePath = TEXT("/Users/remap/Desktop/SaveTimes.txt");
////    FileContent = TEXT("");
////    for (auto Item : cropPoints)
////    {
////        FileContent += FString::SanitizeFloat(Item) + TEXT("\n");
////    }
////    FFileHelper::SaveStringToFile(FileContent, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
//
    return true;
}

bool ASaveDataManager::SavePoints()
{
//    SaveDir = SaveDir.TrimStartAndEnd();
//    // do some sanity checks here
//    if (!SaveDir.EndsWith(FString("/")) or !SaveDir.EndsWith(FString("\\")))
//    {
//        SaveDir = SaveDir.Append(FString("/"));
//    }
//    FString FilePath = SaveDir;
//    FString FileContent = TEXT("");
//
//    for (auto& Item : cropPoints)
//    {
//        FilePath = SaveDir + FString("Python/Crop_") + FString::FromInt(Item.Key) + FString(".txt");
//        FVector4 points = Item.Value;
//        FileContent += FString::SanitizeFloat(points.X) + FString(" ") + FString::SanitizeFloat(points.Y) + FString(" ") + FString::SanitizeFloat(points.Z) + FString(" ") + FString::SanitizeFloat(points.W);
//    }
//    FFileHelper::SaveStringToFile(FileContent, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get());
//
    return true;
}
