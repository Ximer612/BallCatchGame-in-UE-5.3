// Copyright Epic Games, Inc. All Rights Reserved.

#include "BallCatchGameGameMode.h"
#include "BallCatchGameCharacter.h"
#include "EnemyAIController.h"
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

	EnemiesToStun = 0;
}

void ABallCatchGameGameMode::BeginPlay()
{
	Super::BeginPlay();

	ABallCatchGameCharacter* Player = Cast<ABallCatchGameCharacter>(GetWorld()->GetFirstPlayerController()->GetPawn());

	if (Player)
	{
		Player->OnPowerUpStart.BindLambda([this]() {OnPlayerPowerUpStart.Broadcast(); });
		Player->OnPowerUpEnd.BindLambda([this]() {OnPlayerPowerUpEnd.Broadcast(); });
	}

	for (TActorIterator<AEnemyAIController> It(GetWorld()); It; ++It)
	{
		OnPlayerPowerUpStart.AddLambda([It]() { It->EscapeFromPlayer(); UE_LOG(LogTemp, Warning, TEXT("On Player OnPlayerPowerUpStart On Enemy!")); });
		OnPlayerPowerUpEnd.AddLambda([It]() { It->ResumeSearch(); UE_LOG(LogTemp, Warning, TEXT("On Player OnPlayerPowerUpEnd On Enemy!")); });
		EnemiesToStun++;
	}

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
	AttachBallZOffset += 10.0f;
	return AttachBallZOffset;
}

const TArray<class ABall*>& ABallCatchGameGameMode::GetGameBalls() const
{
	return GameBalls;
}

void ABallCatchGameGameMode::DecreaseEnemiesToStunCount()
{
	CurrentEnemiesToStun--;
	if (CurrentEnemiesToStun <= 0)
	{
		ResetMatch();
	}
}

void ABallCatchGameGameMode::ResetMatch()
{
	AttachBallZOffset = 10.f;
	CurrentEnemiesToStun = EnemiesToStun;
	TargetPoints.Empty();
	GameBalls.Empty();

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		TargetPoints.Add(*It);
	}

	//Reset GameBalls
	for (TActorIterator<ABall> It(GetWorld()); It; ++It)
	{
		It->SetActorLocation({0.f,0.f,0.f });
		It->SetActorHiddenInGame(false);
		It->SetActorScale3D({ 0.3f,0.3f,0.3f });
		It->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		GameBalls.Add(*It);
	}

	TArray<ATargetPoint*> RandomTargetPoints = TargetPoints;

	for (int32 i = 0; i < GameBalls.Num(); i++)
	{
		const int32 RandomTargetIndex = FMath::RandRange(0, RandomTargetPoints.Num() - 1);
		GameBalls[i]->SetActorLocation(RandomTargetPoints[RandomTargetIndex]->GetActorLocation());
		RandomTargetPoints.RemoveAt(RandomTargetIndex);
	}

	OnResetMatch.Broadcast();
}
