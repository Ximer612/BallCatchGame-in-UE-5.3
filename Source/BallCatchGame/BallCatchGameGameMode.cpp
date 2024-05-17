// Copyright Epic Games, Inc. All Rights Reserved.

#include "BallCatchGameGameMode.h"
#include "BallCatchGameCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Ball.h"
#include "Engine/TargetPoint.h"


ABallCatchGameGameMode::ABallCatchGameGameMode()
{
	// set default pawn class to our Blueprinted character
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/Blueprints/BP_MainCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

void ABallCatchGameGameMode::BeginPlay()
{
	Super::BeginPlay();

	ResetMatch();
}

void ABallCatchGameGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (int32 i = 0; i < GameBalls.Num(); i++)
	{
		if (GameBalls[i]->GetAttachParentActor() != GetWorld()->GetFirstPlayerController()->GetPawn())
		{
			return;
		}
	}

	ResetMatch();
}

float ABallCatchGameGameMode::GetAttachBallOffset()
{
	AttachBallZOffset += 40.0f;
	return AttachBallZOffset - 40.0f;
}

const TArray<class ABall*>& ABallCatchGameGameMode::GetGameBalls() const
{
	return GameBalls;
}

void ABallCatchGameGameMode::ResetMatch()
{
	AttachBallZOffset = 0.f;
	TargetPoints.Empty();
	GameBalls.Empty();

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		TargetPoints.Add(*It);
	}

	for (TActorIterator<ABall> It(GetWorld()); It; ++It)
	{
		It->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		GameBalls.Add(*It);
	}

	TArray<ATargetPoint*> RandomTargetPoints = TargetPoints;

	for (int32 i = 0; i < GameBalls.Num(); i++)
	{
		const int32 RandomTargetIndex = FMath::RandRange(0, RandomTargetPoints.Num() - 1);
		GameBalls[i]->SetActorLocation(RandomTargetPoints[RandomTargetIndex]->GetActorLocation());
		UE_LOG(LogTemp, Warning, TEXT("Random number -> %d random target -> %p"), RandomTargetIndex, RandomTargetPoints[RandomTargetIndex]);
		RandomTargetPoints.RemoveAt(RandomTargetIndex);
	}

	OnResetMatch.Broadcast();
}
