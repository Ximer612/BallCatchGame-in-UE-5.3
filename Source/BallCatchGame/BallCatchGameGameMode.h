// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BallCatchGameGameMode.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(BallCatchGameCatchGameModeLog, Log, All);

DECLARE_MULTICAST_DELEGATE(FResetMatchDelegate)
DECLARE_MULTICAST_DELEGATE(FOnCatchBallDelegate)

class ATargetPoint;
class ABall;

UCLASS(minimalapi)
class ABallCatchGameGameMode : public AGameModeBase, public FSelfRegisteringExec
{
	GENERATED_BODY()

protected:

	const float StartAttachBallZOffset = 10.0f;
	float AttachBallZOffset;

	int32 EnemiesToStun = 0;
	int32 CurrentEnemiesToStun;

	int32 MaxPlayerHP = 3;
	int32 CurrentPlayerHP;

	TArray<FVector> ActorsSpawnLocations;
	TArray<ATargetPoint*> TargetPoints;
	TArray<ABall*> GameBalls;


	void ResetMatch(const bool bPlayerHasWin = false);

public:
	ABallCatchGameGameMode();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	float GetNewAttachBallOffset();
	const TArray<ABall*>& GetGameBalls() const;
	void DecreaseEnemiesToStunCount();

	FResetMatchDelegate OnResetMatch;
	FOnCatchBallDelegate OnPlayerPowerUpStart;
	FOnCatchBallDelegate OnPlayerPowerUpEnd;

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
};


