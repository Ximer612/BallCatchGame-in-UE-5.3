// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"
#include "Ball.h"
#include "Navigation/PathFollowingComponent.h"
#include "BallCatchGameGameMode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

AEnemyAIController::AEnemyAIController()
{
	UBlackboardComponent* ReturnComponent;
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));

	//ADD ENTRIES TO BLACKBOARD DATA
	BlackboardData = NewObject<UBlackboardData>();

	BestBallObjectType = NewObject<UBlackboardKeyType_Object>();
	BestBallObjectType->BaseClass = AActor::StaticClass();
	FBlackboardEntry BestBallEntry;
	BestBallEntry.EntryName = "BestBall";
	BestBallEntry.KeyType = BestBallObjectType;
	BlackboardData->Keys.Add(std::move(BestBallEntry));

	PathFollowingEnumType = NewObject<UBlackboardKeyType_Enum>();
	PathFollowingEnumType->EnumType = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPathFollowingResult"), true);
	FBlackboardEntry PathFollowingEntry;
	PathFollowingEntry.EntryName = "PathResult";
	PathFollowingEntry.KeyType = PathFollowingEnumType;
	BlackboardData->Keys.Add(std::move(PathFollowingEntry));

	PlayerObjectType = NewObject<UBlackboardKeyType_Object>();
	PlayerObjectType->BaseClass = AActor::StaticClass();
	FBlackboardEntry PlayerEntry;
	PlayerEntry.EntryName = "Player";
	PlayerEntry.KeyType = PlayerObjectType;
	BlackboardData->Keys.Add(std::move(PlayerEntry));

	//

	UseBlackboard(BlackboardData, ReturnComponent);
	Blackboard = ReturnComponent;
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	ReceiveMoveCompleted.AddDynamic(this, &AEnemyAIController::SetBlackboardPathFollowingResult);
	Blackboard->SetValueAsObject("Player", GetWorld()->GetFirstPlayerController()->GetPawn());

	GoToPlayer = MakeShared<FAivState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			AActor* Player = Cast<AActor>(InBlackboard->GetValueAsObject("Player"));
			if (Player)
			{
				AIController->MoveToActor(Player, 5.0f);
			}
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FAivState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}

			if (InBlackboard->GetValueAsEnum("PathResult") == EPathFollowingResult::Success)
			{
				UObject* BestBall = InBlackboard->GetValueAsObject("BestBall");

				if (BestBall)
				{
					AActor* BallActor = Cast<AActor>(BestBall);
					AActor* Player = Cast<AActor>(InBlackboard->GetValueAsObject("Player"));
					if (Player)
					{
						BallActor->AttachToActor(Player, FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));

						AGameModeBase* GameMode = AIController->GetWorld()->GetAuthGameMode();
						ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);

						BallActor->SetActorRelativeLocation(FVector(0, 0, AIGameMode->GetAttachBallOffset()));
					}
				}

				return SearchForBall;
			}

			return GoToRandomPosition;
		}
	);

	SearchForBall = MakeShared<FAivState>(
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard) {

			AGameModeBase* GameMode = AIController->GetWorld()->GetAuthGameMode();
			ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);
			const TArray<ABall*>& BallsList = AIGameMode->GetGameBalls();

			AActor* NearestBall = nullptr;

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

			InBlackboard->SetValueAsObject("BestBall", NearestBall);
			//UE_LOG(LogTemp, Warning, TEXT("BestBall = %p"), Blackboard->GetValueAsObject("BestBall"));

			InBlackboard->SetValueAsObject("BestBall", NearestBall);
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FAivState> {

			if (InBlackboard->GetValueAsObject("BestBall"))
			{
				return GoToBall;
			}
			else {
				return WaitNextRoundStart;
			}
		}
	);

	GoToBall = MakeShared<FAivState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			UObject* BestBall = InBlackboard->GetValueAsObject("BestBall");
			AActor* BallActor = Cast<AActor>(BestBall);
			AIController->MoveToActor(BallActor, 100.0f);
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FAivState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				UObject* BestBall = InBlackboard->GetValueAsObject("BestBall");
				AActor* BallActor = Cast<AActor>(BestBall);

				if (BallActor->GetAttachParentActor())
				{
					return SearchForBall;
				}

				return nullptr;
			}
			return GrabBall;
		}
	);

	GrabBall = MakeShared<FAivState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard)
		{
			UObject* BestBall = InBlackboard->GetValueAsObject("BestBall");
			AActor* BallActor = Cast<AActor>(BestBall);
			if (BallActor->GetAttachParentActor())
			{
				InBlackboard->SetValueAsObject("BestBall", nullptr);
			}
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FAivState> {

			UObject* BestBall = InBlackboard->GetValueAsObject("BestBall");
			if (!BestBall)
			{
				return SearchForBall;
			}

			AActor* BallActor = Cast<AActor>(BestBall);
			BallActor->AttachToActor(AIController->GetPawn(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));
			BallActor->SetActorRelativeLocation(FVector(0, 0, 0));

			return GoToPlayer;
		}
	);

	StartLocation = GetPawn()->GetActorLocation();

	GoToRandomPosition = MakeShared<FAivState>(
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			if (NavSystem)
			{
				FNavLocation ResultLocation;
				bool canBeReached = NavSystem->GetRandomReachablePointInRadius(StartLocation, 10000.0f, ResultLocation);
				if (canBeReached)
				{
					AIController->MoveToLocation(ResultLocation.Location, 250.0f);
				}
			}
				//AIController->MoveToLocation(StartLocation, 250.0f);
			UE_LOG(LogTemp, Warning, TEXT("NAV SYS addio"));

		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FAivState> {

			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

			AActor* Player = Cast<AActor>(InBlackboard->GetValueAsObject("Player"));
			if (Player)
			{
				UNavigationPath* NavPath = NavSystem->FindPathToLocationSynchronously(GetWorld(), AIController->GetPawn()->GetActorLocation(), Player->GetActorLocation(), NULL);
				if (NavPath->IsValid())
				{
					return GoToPlayer;
				}
			}

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}

			if (NavSystem)
			{
				FNavLocation ResultLocation;
				bool canBeReached = NavSystem->GetRandomReachablePointInRadius(StartLocation, 10000.0f, ResultLocation);
				if (canBeReached)
				{
					AIController->MoveToLocation(ResultLocation.Location, 100.0f);
				}
			}

			//return GoToPlayer;
			return nullptr;
		}
	);

	WaitNextRoundStart = MakeShared<FAivState>(
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			AIController->MoveToLocation(StartLocation, 500.0f);
		},
		nullptr,
		nullptr
	);

	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);

	AIGameMode->OnResetMatch.AddLambda(
		[this]() -> void {
			CurrentState = SearchForBall;
		});

	CurrentState = SearchForBall;
	CurrentState->CallEnter(this, Blackboard);
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState = CurrentState->CallTick(this,Blackboard, DeltaTime);
	}

}

void AEnemyAIController::SetBlackboardPathFollowingResult(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	Blackboard->SetValueAsEnum("PathResult", Result);

}
