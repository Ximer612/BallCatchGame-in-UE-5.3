// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BallGameInterface.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyAIController.generated.h"

struct FStateMachineState : public TSharedFromThis<FStateMachineState>
{
private:
	TFunction<void(AAIController*, UBlackboardComponent*)> Enter;
	TFunction<void(AAIController*, UBlackboardComponent*)> Exit;
	TFunction<TSharedPtr<FStateMachineState>(AAIController*, UBlackboardComponent*, const float)> Tick;
public:

	FStateMachineState()
	{
		Enter = nullptr;
		Exit = nullptr;
		Tick = nullptr;
	}

	FStateMachineState(TFunction<void(AAIController*, UBlackboardComponent*)> InEnter = nullptr, TFunction<void(AAIController*, UBlackboardComponent*)> InExit = nullptr, TFunction<TSharedPtr<FStateMachineState>(AAIController*, UBlackboardComponent*, const float)> InTick = nullptr)
	{
		Enter = InEnter;
		Exit = InExit;
		Tick = InTick;
	}

	FStateMachineState(const FStateMachineState& Other) = delete;
	FStateMachineState& operator=(const FStateMachineState& Other) = delete;
	FStateMachineState(FStateMachineState&& Other) = delete;
	FStateMachineState& operator=(FStateMachineState&& Other) = delete;

	void CallEnter(AAIController* AIController, UBlackboardComponent* Blackboard)
	{
		if (Enter)
		{
			Enter(AIController, Blackboard);
		}
	}

	void CallExit(AAIController* AIController, UBlackboardComponent* Blackboard)
	{
		if (Exit)
		{
			Exit(AIController, Blackboard);
		}
	}

	TSharedPtr<FStateMachineState> CallTick(AAIController* AIController, UBlackboardComponent* Blackboard, const float DeltaTime)
	{
		if (Tick)
		{
			TSharedPtr<FStateMachineState> NewState = Tick(AIController, Blackboard, DeltaTime);

			if (NewState != nullptr && NewState != AsShared())
			{
				CallExit(AIController, Blackboard);
				NewState->CallEnter(AIController, Blackboard);
				return NewState;
			}
		}

		return AsShared();
	}
};


DECLARE_LOG_CATEGORY_EXTERN(BallCatchGameEnemyAIControllerLog, Log, All);

class UBlackboardKeyType_Object;
class UBlackboardKeyType_Vector;
class UBlackboardKeyType_Bool;
class UBlackboardKeyType_Enum;

/**
 * 
 */
UCLASS()
class BALLCATCHGAME_API AEnemyAIController : public AAIController, public IBallGameInterface
{
	GENERATED_BODY()
	
protected:
	TSharedPtr<FStateMachineState> CurrentState;
	TSharedPtr<FStateMachineState> GoToPlayerState;
	TSharedPtr<FStateMachineState> GoToBallState;
	TSharedPtr<FStateMachineState> GrabBallState;
	TSharedPtr<FStateMachineState> SearchForBallState;
	TSharedPtr<FStateMachineState> GoToRandomPositionState;
	TSharedPtr<FStateMachineState> IdleUntilNextRoundState;
	TSharedPtr<FStateMachineState> EscapeFromPlayerState;

	AEnemyAIController();
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	FVector StartLocation;

	const float EscapeWalkSpeed = 400.0f;
	const float DefaultWalkSpeed = 500.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBlackboardData> BlackboardData;

	TObjectPtr<UBlackboardKeyType_Object> BestBallObjectType;
	TObjectPtr<UBlackboardKeyType_Object> PlayerObjectType;
	TObjectPtr<UBlackboardKeyType_Enum> PathFollowingEnumType;
	TObjectPtr<UBlackboardKeyType_Vector> VectorType;
	TObjectPtr<UBlackboardKeyType_Bool> BoolType;

	UFUNCTION() //needed to be binded to a delegate
	void SetBlackboardPathFollowingResult(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	void SwitchStateMachineState(TSharedPtr<FStateMachineState> NewState, bool bBypassIdleState = false);


public:
	void EscapeFromPlayer();
	void ResumeSearch();

	bool Stun_Implementation() override;
};
