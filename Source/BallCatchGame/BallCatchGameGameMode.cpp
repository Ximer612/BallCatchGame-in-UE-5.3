// Copyright Epic Games, Inc. All Rights Reserved.

#include "BallCatchGameGameMode.h"
#include "BallCatchGameCharacter.h"
#include "EnemyAIController.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Ball.h"
#include "Engine/TargetPoint.h"

DEFINE_LOG_CATEGORY(LogBallCatchGameMode);


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

	APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();

	if (!PlayerPawn)
	{
		return;
	}

	ABallCatchGameCharacter* Player = Cast<ABallCatchGameCharacter>(PlayerPawn);

	if (Player)
	{
		Player->OnPowerUpStart.AddLambda([this]() {OnPlayerPowerUpStart.Broadcast(); });
		Player->OnPowerUpEnd.AddLambda([this]() {OnPlayerPowerUpEnd.Broadcast(); });
		ActorsSpawnLocations.Add(PlayerPawn->GetActorLocation());
		OnResetMatch.AddLambda([Player]() {Player->CallPowerUpEnd(); });
	}

	for (TActorIterator<AEnemyAIController> It(GetWorld()); It; ++It)
	{
		OnPlayerPowerUpStart.AddLambda([It]() { It->EscapeFromPlayer(); UE_LOG(LogBallCatchGameMode, Warning, TEXT("On Player OnPlayerPowerUpStart On Enemy!")); });
		OnPlayerPowerUpEnd.AddLambda([It]() { It->ResumeSearch(); UE_LOG(LogBallCatchGameMode, Warning, TEXT("On Player OnPlayerPowerUpEnd On Enemy!")); });
		EnemiesToStun++;
		ActorsSpawnLocations.Add(It->GetPawn()->GetActorLocation());
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

	//Called if every game balls has been attached to player
	ResetMatch(false);
}

float ABallCatchGameGameMode::GetNewAttachBallOffset()
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
		ResetMatch(true);
		//Player win!
	}
}

bool ABallCatchGameGameMode::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("ResetMatch")))
	{
		if (InWorld->WorldType == EWorldType::PIE)
		{
			ABallCatchGameGameMode* CurrentGameMode = Cast<ABallCatchGameGameMode>(InWorld->GetAuthGameMode());

			if(FParse::Command(&Cmd, TEXT("Win")))
			{
				CurrentGameMode->ResetMatch(true);
				return true;
			}

			CurrentGameMode->ResetMatch();
			return true;
		}
		return true;
	}

	return false;
}

void ABallCatchGameGameMode::ResetMatch(const bool bPlayerHasWin)
{

	if (bPlayerHasWin)
	{
		UE_LOG(LogBallCatchGameMode, Warning, TEXT("PLAYER HAS WIN!!!"));
	}

	AttachBallZOffset = StartAttachBallZOffset;
	CurrentEnemiesToStun = EnemiesToStun;

	TargetPoints.Empty();
	GameBalls.Empty();

	//RESET ACTORS LOCATIONS
	APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();

	if (!PlayerPawn)
	{
		return;
	}
		PlayerPawn->SetActorLocation(ActorsSpawnLocations[0]);

	for (TActorIterator<AEnemyAIController> It(GetWorld()); It; ++It)
	{
		It->GetPawn()->SetActorLocation(ActorsSpawnLocations[It.GetProgressNumerator()]);
	}

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
