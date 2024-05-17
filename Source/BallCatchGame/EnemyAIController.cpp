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

	//ADD BEST BALL ENTRY TO BLACKBOARD DATA
	BlackboardData = NewObject<UBlackboardData>();
	BestBallType = NewObject<UBlackboardKeyType_Object>();
	BestBallType->BaseClass = AActor::StaticClass();
	FBlackboardEntry BestBallEntry;
	BestBallEntry.EntryName = "BestBall";
	BestBallEntry.KeyType = BestBallType;
	BlackboardData->Keys.Add(std::move(BestBallEntry));
	//

	UseBlackboard(BlackboardData, ReturnComponent);
	Blackboard = ReturnComponent;
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	ReceiveMoveCompleted.AddDynamic(this, &AEnemyAIController::TestFunc);

	GoToPlayer = MakeShared<FAivState>(
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			AIController->MoveToActor(AIController->GetWorld()->GetFirstPlayerController()->GetPawn(), 10.0f);


		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FAivState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}

			//UE_LOG(LogTemp, Warning, TEXT("Variabile: %s"), State);

			// add that the player can get a ball, enemy run away
			
			//FAIRequestID RequestID;
			//const FPathFollowingResult Result(EPathFollowingResult::Aborted,FPathFollowingResultFlags::None);
			//AIController->OnMoveCompleted(RequestID, Result)

			UObject* BestBall = InBlackboard->GetValueAsObject("BestBall");

			if (BestBall)
			{
				AActor* BallActor = Cast<AActor>(BestBall);
				BallActor->AttachToActor(AIController->GetWorld()->GetFirstPlayerController()->GetPawn(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));
				
				AGameModeBase* GameMode = AIController->GetWorld()->GetAuthGameMode();
				ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);
				
				BallActor->SetActorRelativeLocation(FVector(0, 0, AIGameMode->GetAttachBallOffset()));
			}

			return SearchForBall;
		}
	);

	SearchForBall = MakeShared<FAivState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
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
				return GoToStartPosition;
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

	GoToStartPosition = MakeShared<FAivState>(
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			AIController->MoveToLocation(StartLocation, 250.0f);
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

void AEnemyAIController::TestFunc(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPathFollowingResult"), true);
	//if (!EnumPtr) return NSLOCTEXT("Invalid", "Invalid", "Invalid");

	UE_LOG(LogTemp, Warning, TEXT("Variabile: %s"), *(EnumPtr->GetDisplayNameTextByValue(Result).ToString()) );

	if (Result == EPathFollowingResult::Aborted || Result == EPathFollowingResult::Invalid)
	{
		CurrentState = SearchForBall;
	}

}
