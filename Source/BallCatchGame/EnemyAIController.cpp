// Fill out your copyright notice in the Description page of Project Settings.
#include "EnemyAIController.h"
#include "Ball.h"
#include "BallCatchGameGameMode.h"

#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY(LogEnemyAIController);

AEnemyAIController::AEnemyAIController()
{
	UBlackboardComponent* NewBlackboard;
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("Blackboard Component"));

	//ADD ENTRIES TO BLACKBOARD DATA
	BlackboardData = NewObject<UBlackboardData>();
	//BlackboardData = BlackboardData = CreateDefaultSubobject<UBlackboardData>(TEXT("Blackboard Data"));

	BestBallObjectType = NewObject<UBlackboardKeyType_Object>();
	BestBallObjectType->BaseClass = AActor::StaticClass();
	FBlackboardEntry BestBallEntry;
	BestBallEntry.EntryName = TEXT("BestBall");
	BestBallEntry.KeyType = BestBallObjectType;
	BlackboardData->Keys.Add(std::move(BestBallEntry));

	PathFollowingEnumType = NewObject<UBlackboardKeyType_Enum>();
	PathFollowingEnumType->EnumType = FindFirstObjectSafe<UEnum>(TEXT("EPathFollowingResult"));
	FBlackboardEntry PathFollowingEntry;
	PathFollowingEntry.EntryName = TEXT("PathResult");
	PathFollowingEntry.KeyType = PathFollowingEnumType;
	BlackboardData->Keys.Add(std::move(PathFollowingEntry));

	PlayerObjectType = NewObject<UBlackboardKeyType_Object>();
	PlayerObjectType->BaseClass = AActor::StaticClass();
	FBlackboardEntry PlayerEntry;
	PlayerEntry.EntryName = TEXT("Player");
	PlayerEntry.KeyType = PlayerObjectType;
	BlackboardData->Keys.Add(std::move(PlayerEntry));

	VectorType = NewObject<UBlackboardKeyType_Vector>();
	FBlackboardEntry StartLocationTypeEntry;
	StartLocationTypeEntry.EntryName = TEXT("StartLocation");
	StartLocationTypeEntry.KeyType = VectorType;
	BlackboardData->Keys.Add(std::move(StartLocationTypeEntry));

	BoolType = NewObject<UBlackboardKeyType_Bool>();
	FBlackboardEntry HasBallTypeEntry;
	HasBallTypeEntry.EntryName = TEXT("bHasBall");
	HasBallTypeEntry.KeyType = BoolType;
	BlackboardData->Keys.Add(std::move(HasBallTypeEntry));

	FBlackboardEntry IsEscapingEntry;
	IsEscapingEntry.EntryName = TEXT("bIsEscaping");
	IsEscapingEntry.KeyType = BoolType;
	BlackboardData->Keys.Add(std::move(IsEscapingEntry));

	//

	UseBlackboard(BlackboardData, NewBlackboard);
	Blackboard = NewBlackboard;
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	ReceiveMoveCompleted.AddDynamic(this, &AEnemyAIController::SetBlackboardPathFollowingResult);

	Blackboard->SetValueAsObject(TEXT("Player"), GetWorld()->GetFirstPlayerController()->GetPawn());
	Blackboard->SetValueAsVector(TEXT("StartLocation"), GetPawn()->GetActorLocation());

	GoToPlayerState = MakeShared<FStateMachineState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			//UE_LOG(LogEnemyAIController, Warning, TEXT("GoToPlayerState!!!"));
			AActor* Player = Cast<AActor>(InBlackboard->GetValueAsObject(TEXT("Player")));
			if (Player)
			{
				AIController->MoveToActor(Player, 5.0f);
			}
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FStateMachineState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}

			if (InBlackboard->GetValueAsEnum(TEXT("PathResult")) == EPathFollowingResult::Success)
			{
				UObject* BestBall = InBlackboard->GetValueAsObject(TEXT("BestBall"));

				if (BestBall)
				{
					AActor* BallActor = Cast<AActor>(BestBall);
					ACharacter* Player = Cast<ACharacter>(InBlackboard->GetValueAsObject(TEXT("Player")));
					if (Player)
					{
						BallActor->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("head"));

						AGameModeBase* GameMode = AIController->GetWorld()->GetAuthGameMode();
						ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);

						BallActor->SetActorRelativeLocation(FVector(AIGameMode->GetNewAttachBallOffset(), 0, 0));
						BallActor->SetActorScale3D({ 0.1f,0.1f,0.1f });
						InBlackboard->SetValueAsBool(TEXT("bHasBall"), false);
					}
				}

				return SearchForBallState;
			}

			return GoToRandomPositionState;
		}
	);

	SearchForBallState = MakeShared<FStateMachineState>(
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

			InBlackboard->SetValueAsObject(TEXT("BestBall"), NearestBall);
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FStateMachineState> {

			if (InBlackboard->GetValueAsObject(TEXT("BestBall")))
			{
				return GoToBallState;
			}
			else {
				return IdleUntilNextRoundState;
			}
		}
	);

	GoToBallState = MakeShared<FStateMachineState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			//UE_LOG(LogEnemyAIController, Warning, TEXT("GoToBallState!!!"));
			UObject* BestBall = InBlackboard->GetValueAsObject(TEXT("BestBall"));
			AActor* BallActor = Cast<AActor>(BestBall);
			AIController->MoveToActor(BallActor, 100.0f);
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FStateMachineState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				UObject* BestBall = InBlackboard->GetValueAsObject(TEXT("BestBall"));
				AActor* BallActor = Cast<AActor>(BestBall);

				if (BallActor->GetAttachParentActor())
				{
					return SearchForBallState;
				}

				return nullptr;
			}
			return GrabBallState;
		}
	);

	GrabBallState = MakeShared<FStateMachineState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard)
		{
			//UE_LOG(LogEnemyAIController, Warning, TEXT("GrabBallState!!!"));
			UObject* BestBall = InBlackboard->GetValueAsObject(TEXT("BestBall"));
			AActor* BallActor = Cast<AActor>(BestBall);
			if (BallActor->GetAttachParentActor())
			{
				InBlackboard->SetValueAsObject(TEXT("BestBall"), nullptr);
			}
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FStateMachineState> {

			UObject* BestBall = InBlackboard->GetValueAsObject(TEXT("BestBall"));
			if (!BestBall)
			{
				return SearchForBallState;
			}

			AActor* BallActor = Cast<AActor>(BestBall);
			InBlackboard->SetValueAsBool(TEXT("bHasBall"), true);

			ACharacter* Character = Cast<ACharacter>(AIController->GetPawn());
			if(Character)
			{
				BallActor->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));
			}

			BallActor->SetActorRelativeLocation(FVector(0, 0, 0));

			return GoToPlayerState;
		}
	);

	GoToRandomPositionState = MakeShared<FStateMachineState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			//UE_LOG(LogEnemyAIController, Warning, TEXT("GoToRandomPositionState!!!"));
			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(AIController->GetWorld());
			if (NavSystem)
			{
				FNavLocation ResultLocation;
				bool canBeReached = NavSystem->GetRandomReachablePointInRadius(InBlackboard->GetValueAsVector(TEXT("StartLocation")), 10000.0f, ResultLocation);
				if (canBeReached)
				{
					AIController->MoveToLocation(ResultLocation.Location, 250.0f);
				}
			}
		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FStateMachineState> {

			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

			AActor* Player = Cast<AActor>(InBlackboard->GetValueAsObject(TEXT("Player")));
			if (Player)
			{
				UNavigationPath* NavPath = NavSystem->FindPathToLocationSynchronously(GetWorld(), AIController->GetPawn()->GetActorLocation(), Player->GetActorLocation(), NULL);
				if (NavPath->IsValid())
				{
					return GoToPlayerState;
				}
			}

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}

			if (NavSystem)
			{
				FNavLocation ResultLocation;
				bool canBeReached = NavSystem->GetRandomReachablePointInRadius(InBlackboard->GetValueAsVector(TEXT("StartLocation")), 10000.0f, ResultLocation);
				if (canBeReached)
				{
					AIController->MoveToLocation(ResultLocation.Location, 100.0f);
				}
			}
			return nullptr;
		}
	);

	EscapeFromPlayerState = MakeShared<FStateMachineState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			//UE_LOG(LogEnemyAIController, Warning, TEXT("EscapeFromPlayerState!!!"));
			AActor* Player = Cast<AActor>(InBlackboard->GetValueAsObject(TEXT("Player")));

			if (Player)
			{
				FVector DirectionToMove = AIController->GetPawn()->GetActorLocation() - Player->GetActorLocation();
				DirectionToMove.Normalize();
				FVector NewLocation = DirectionToMove * 100.0f;
				AIController->MoveToLocation(AIController->GetPawn()->GetActorLocation() + NewLocation, 20.0f);
			}

		},
		nullptr,
		[this](AAIController* AIController, UBlackboardComponent* InBlackboard, const float DeltaTime) -> TSharedPtr<FStateMachineState> {

			if (!InBlackboard->GetValueAsBool(TEXT("bIsEscaping")))
			{
				if (InBlackboard->GetValueAsBool(TEXT("bHasBall")))
				{
					return GoToPlayerState;
				}
				else {
					return SearchForBallState;
				}
			}

			EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}

			AActor* Player = Cast<AActor>(InBlackboard->GetValueAsObject(TEXT("Player")));

			if (Player)
			{
				FVector DirectionToMove = AIController->GetPawn()->GetActorLocation() - Player->GetActorLocation();
				DirectionToMove.Normalize();
				FVector NewLocation = DirectionToMove * 100.0f;
				AIController->MoveToLocation(AIController->GetPawn()->GetActorLocation() + NewLocation, 20.0f);
			}

			return nullptr;
		}
	);

	IdleUntilNextRoundState = MakeShared<FStateMachineState>(
		[](AAIController* AIController, UBlackboardComponent* InBlackboard) {
			//UE_LOG(LogEnemyAIController, Warning, TEXT("IdleUntilNextRoundState!!!"));
			AIController->MoveToLocation(InBlackboard->GetValueAsVector(TEXT("StartLocation")), 500.0f);
		},
		nullptr,
		nullptr
	);

	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);

	AIGameMode->OnResetMatch.AddLambda(
		[this]() -> void {
			//UE_LOG(LogEnemyAIController, Warning, TEXT("OnResetMatch!!!"));
			SwitchStateMachineState(SearchForBallState,true);
		});

	CurrentState = SearchForBallState;
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
	Blackboard->SetValueAsEnum(TEXT("PathResult"), Result);

}

void AEnemyAIController::SwitchStateMachineState(TSharedPtr<FStateMachineState> NewState, bool bBypassIdleState)
{
	if (!bBypassIdleState)
	{
		if (CurrentState == IdleUntilNextRoundState) return;
	}

	CurrentState->CallExit(this, Blackboard);
	CurrentState = NewState;
	CurrentState->CallEnter(this, Blackboard);
}

void AEnemyAIController::EscapeFromPlayer()
{
	//UE_LOG(LogEnemyAIController, Warning, TEXT("ESCAPE FROM PLAYER!!!"));

	SwitchStateMachineState(EscapeFromPlayerState);
	Blackboard->SetValueAsBool(TEXT("bIsEscaping"), true);

	GetCharacter()->GetCharacterMovement()->MaxWalkSpeed = EscapeWalkSpeed;
}

void AEnemyAIController::ResumeSearch()
{
	//UE_LOG(LogEnemyAIController, Warning, TEXT("RESUME SEARCH!!!!"));
	Blackboard->SetValueAsBool(TEXT("bIsEscaping"), false);

	GetCharacter()->GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
}

bool AEnemyAIController::Stun_Implementation()
{
	if (CurrentState == IdleUntilNextRoundState)
	{
		return true;
	}

	SwitchStateMachineState(IdleUntilNextRoundState);

	if (Blackboard->GetValueAsBool(TEXT("bHasBall")))
	{
		AActor* BallActor = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("BestBall")));
		if (BallActor)
		{
			BallActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("STUNNED!!!!"));

	return false;
}
