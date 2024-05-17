// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BallCatchGameGameMode.generated.h"

DECLARE_MULTICAST_DELEGATE(FResetMatchDelegate)
DECLARE_MULTICAST_DELEGATE(FOnCatchBallDelegate)

UCLASS(minimalapi)
class ABallCatchGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	float AttachBallZOffset;
	int32 EnemiesToStun;
	int32 CurrentEnemiesToStun;

	TArray<class ATargetPoint*> TargetPoints;
	TArray<class ABall*> GameBalls;

	void ResetMatch();

public:
	ABallCatchGameGameMode();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	float GetAttachBallOffset();
	const TArray<class ABall*>& GetGameBalls() const;
	void DecreaseEnemiesToStunCount();

	FResetMatchDelegate OnResetMatch;
	FOnCatchBallDelegate OnPlayerPowerUpStart;
	FOnCatchBallDelegate OnPlayerPowerUpEnd;

};


