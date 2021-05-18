// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCorePlane.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AVideoCorePlane::AVideoCorePlane(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	//Super(ObjectInitializer.SetDefaultSubobjectClass<UPlaneMovementComponent>(ADefaultPawn::MovementComponentName))
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	planeMovementComponent_ = Cast<UPlaneMovementComponent>(this->MovementComponent);

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

UPawnMovementComponent* AVideoCorePlane::GetMovementComponent() const
{
	if (!planeMovementComponent_)
		return FindComponentByClass<UPlaneMovementComponent>();

	return planeMovementComponent_;
}

void AVideoCorePlane::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}