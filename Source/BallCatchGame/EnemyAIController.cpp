// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"
#include "Ball.h"
#include "Navigation/PathFollowingComponent.h"
#include "BallCatchGameGameMode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"


AEnemyAIController::AEnemyAIController()
{
	UBlackboardComponent* ReturnComponent;
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
	BlackboardData = CreateDefaultSubobject<UBlackboardData>(TEXT("BlackboardData"));
	BestBallType = CreateDefaultSubobject<UBlackboardKeyType_Object>(TEXT("BestBall"));
	FBlackboardEntry TestIntEntry;
	TestIntEntry.EntryName = "BestBall";
	TestIntEntry.KeyType = BestBallType;
	BlackboardData->Keys.Add(std::move(TestIntEntry));
	UseBlackboard(BlackboardData, ReturnComponent);
	Blackboard = ReturnComponent;
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("SelfActor = %d"), Blackboard->GetValueAsObject("SelfActor"));

	GoToPlayer = MakeShared<FAivState>(
		[](AAIController* AIController) {
			AIController->MoveToActor(AIController->GetWorld()->GetFirstPlayerController()->GetPawn(), 100.0f);
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}

			if (BestBall)
			{
				BestBall->AttachToActor(AIController->GetWorld()->GetFirstPlayerController()->GetPawn(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));
				BestBall->SetActorRelativeLocation(FVector(0, 0, 0));
				BestBall = nullptr;
			}
			return SearchForBall;
		}
	);

	SearchForBall = MakeShared<FAivState>(
		[this](AAIController* AIController) {
			AGameModeBase* GameMode = AIController->GetWorld()->GetAuthGameMode();
			ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);
			const TArray<ABall*>& BallsList = AIGameMode->GetBalls();

			ABall* NearestBall = nullptr;

			for (int32 i = 0; i < BallsList.Num(); i++)
			{
				if (!BallsList[i]->GetAttachParentActor() &&
					(!NearestBall ||
						FVector::Distance(AIController->GetPawn()->GetActorLocation(), BallsList[i]->GetActorLocation()) <
						FVector::Distance(AIController->GetPawn()->GetActorLocation(), NearestBall->GetActorLocation())))
				{
					NearestBall = BallsList[i];
				}
			}

			Blackboard->SetValueAsObject("BestBall", NearestBall);
			UE_LOG(LogTemp, Warning, TEXT("BestBall = %p"), Blackboard->GetValueAsObject("BestBall"));

			BestBall = NearestBall;
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState> {
			if (BestBall)
			{
				return GoToBall;
			}
			else {
				return SearchForBall;
			}
		}
	);

	GoToBall = MakeShared<FAivState>(
		[this](AAIController* AIController) {
			AIController->MoveToActor(BestBall, 100.0f);
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}
			return GrabBall;
		}
	);

	GrabBall = MakeShared<FAivState>(
		[this](AAIController* AIController)
		{
			if (BestBall->GetAttachParentActor())
			{
				BestBall = nullptr;
			}
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState> {

			if (!BestBall)
			{
				return SearchForBall;
			}

			BestBall->AttachToActor(AIController->GetPawn(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));
			BestBall->SetActorRelativeLocation(FVector(0, 0, 0));

			return GoToPlayer;
		}
	);

	CurrentState = SearchForBall;
	CurrentState->CallEnter(this);
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState = CurrentState->CallTick(this, DeltaTime);
	}
}
