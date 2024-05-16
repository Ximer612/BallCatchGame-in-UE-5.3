// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyAIController.generated.h"

struct FAivState : public TSharedFromThis<FAivState>
{
private:
	TFunction<void(AAIController*, UBlackboardComponent*)> Enter;
	TFunction<void(AAIController*, UBlackboardComponent*)> Exit;
	TFunction<TSharedPtr<FAivState>(AAIController*, UBlackboardComponent*, const float)> Tick;
public:

	FAivState()
	{
		Enter = nullptr;
		Exit = nullptr;
		Tick = nullptr;
	}

	FAivState(TFunction<void(AAIController*, UBlackboardComponent*)> InEnter = nullptr, TFunction<void(AAIController*, UBlackboardComponent*)> InExit = nullptr, TFunction<TSharedPtr<FAivState>(AAIController*, UBlackboardComponent*, const float)> InTick = nullptr)
	{
		Enter = InEnter;
		Exit = InExit;
		Tick = InTick;
	}

	FAivState(const FAivState& Other) = delete;
	FAivState& operator=(const FAivState& Other) = delete;
	FAivState(FAivState&& Other) = delete;
	FAivState& operator=(FAivState&& Other) = delete;

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

	TSharedPtr<FAivState> CallTick(AAIController* AIController, UBlackboardComponent* Blackboard, const float DeltaTime)
	{
		if (Tick)
		{
			TSharedPtr<FAivState> NewState = Tick(AIController, Blackboard, DeltaTime);

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

/**
 * 
 */
UCLASS()
class BALLCATCHGAME_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()
	
protected:
	TSharedPtr<FAivState> CurrentState;
	TSharedPtr<FAivState> GoToPlayer;
	TSharedPtr<FAivState> GoToBall;
	TSharedPtr<FAivState> GrabBall;
	TSharedPtr<FAivState> SearchForBall;
	TSharedPtr<FAivState> GoToStartPosition;

	AEnemyAIController();
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	//class ABall* BestBall;

	FVector StartLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBlackboardData> BlackboardData;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UBlackboardKeyType_Object> BestBallType;

};
