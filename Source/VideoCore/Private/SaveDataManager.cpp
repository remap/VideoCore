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
    FString FilePath = TEXT("/Users/remap/Desktop/SaveTransforms.txt");//FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir()) + TEXT("/TextFileTest.txt");
    FString FileContent = TEXT("");
    for (auto Item : recordedTransforms)
    {
        FileContent += Item.ToString() + TEXT("\n");
    }
    FFileHelper::SaveStringToFile(FileContent, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
    //now timesteps
    FilePath = TEXT("/Users/remap/Desktop/SaveTimes.txt");
    FileContent = TEXT("");
    for (auto Item : recordedTimes)
    {
        FileContent += FString::SanitizeFloat(Item) + TEXT("\n");
    }
    FFileHelper::SaveStringToFile(FileContent, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
 
    return true;
}
