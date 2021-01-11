// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCorePlane.h"

// Sets default values
AVideoCorePlane::AVideoCorePlane()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AVideoCorePlane::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AVideoCorePlane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AVideoCorePlane::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AVideoCorePlane::OnDeserializationCompleted_Implementation()
{
}
