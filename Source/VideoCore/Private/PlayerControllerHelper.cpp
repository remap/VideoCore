// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerControllerHelper.h"

// Sets default values for this component's properties
UPlayerControllerHelper::UPlayerControllerHelper()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPlayerControllerHelper::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UPlayerControllerHelper::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void
UPlayerControllerHelper::SpawnPlaneActor(UClass *planeClass, FTransform transform,
                     FOnNewPlaneSpawned onNewPlaneSpawned)
{
    User_onNewPlaneSpawned_ = onNewPlaneSpawned;
    RPC_SpawnPlaneActor(planeClass, transform);
}

void
UPlayerControllerHelper::RPC_SpawnPlaneActor_Implementation(UClass *planeClass, FTransform transform)
{
    AActor *plane = GetWorld()->SpawnActor<APawn>(planeClass, transform);
    
    OnServerSpawnedPlane(plane);
    if (Server_OnNewPlaneSpawned.IsBound())
        Server_OnNewPlaneSpawned.Broadcast(plane);
}

void
UPlayerControllerHelper::CallUserOnNewPlaneSpawned(FString planeName)
{
    if (User_onNewPlaneSpawned_.IsBound())
    {
        User_onNewPlaneSpawned_.Execute(planeName);
        User_onNewPlaneSpawned_.Unbind();
    }
}

void
UPlayerControllerHelper::OnServerSpawnedPlane_Implementation(AActor *plane)
{
    // (implementation when blueprint does not have one)
    // nothing's here
}
