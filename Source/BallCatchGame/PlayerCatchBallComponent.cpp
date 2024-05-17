// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCatchBallComponent.h"

// Sets default values for this component's properties
UPlayerCatchBallComponent::UPlayerCatchBallComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	BallTriggerBox = CreateDefaultSubobject<UBoxComponent>(FName("Collision Mesh"));
	BallTriggerBox->OnComponentBeginOverlap.AddDynamic(this, &UPlayerCatchBallComponent::CatchBall);
	BallTriggerBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	BallTriggerBox->SetBoxExtent({ 80,80,100 });
	// ...
}


// Called when the game starts
void UPlayerCatchBallComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UPlayerCatchBallComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UPlayerCatchBallComponent::CatchBall(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("AIUTOOOOOOO"));

	if (!OtherActor)
	{
		return;
	}

	if (OtherActor->ActorHasTag(TEXT("GameBall")) && !OtherActor->GetAttachParentActor())
	{
		OtherActor->AttachToActor(this->GetOwner(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));
		OtherActor->SetActorRelativeLocation({ 0,0,0 });
	}
}

