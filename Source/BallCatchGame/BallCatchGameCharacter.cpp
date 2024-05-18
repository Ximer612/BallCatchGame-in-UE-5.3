// Copyright Epic Games, Inc. All Rights Reserved.

#include "BallCatchGameCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "BallCatchGameGameMode.h"
#include "BallGameInterface.h"

DEFINE_LOG_CATEGORY(LogAIBallCatchCharacter);

//////////////////////////////////////////////////////////////////////////
// ABallCatchGameCharacter

ABallCatchGameCharacter::ABallCatchGameCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	Tags.Add(TEXT("Player"));

	BallTriggerBox = CreateDefaultSubobject<UBoxComponent>(FName("Ball Catch Trigger Box"));
	BallTriggerBox->InitBoxExtent({ 20,40,70 });
	BallTriggerBox->SetRelativeLocation({30,0,10});
	BallTriggerBox->SetupAttachment(this->GetCapsuleComponent());
	BallTriggerBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	BallTriggerBox->SetGenerateOverlapEvents(true);
	BallTriggerBox->bHiddenInGame = false;
	BallTriggerBox->SetVisibility(true);
	BallTriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABallCatchGameCharacter::CatchBall);

	bCanAttack = false;
}

void ABallCatchGameCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	ABallCatchGameGameMode* AIGameMode = Cast<ABallCatchGameGameMode>(GameMode);
	OnStunEnemy.BindLambda([AIGameMode]() {
		AIGameMode->DecreaseEnemiesToStunCount();
		});

}

void ABallCatchGameCharacter::Tick(float DeltaTime)
{
	AttackingCounter -= DeltaTime;

	if (AttackingCounter < 0)
	{
		SetActorTickEnabled(false);
		bCanAttack = false;
		SetActorScale3D({ 1.f, 1.f,1.f });
		OnPowerUpEnd.ExecuteIfBound();
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ABallCatchGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABallCatchGameCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABallCatchGameCharacter::Look);
	}
	else
	{
		UE_LOG(LogAIBallCatchCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ABallCatchGameCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ABallCatchGameCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ABallCatchGameCharacter::CatchBall(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	if (OtherActor->ActorHasTag(TEXT("GameBall")) && !OtherActor->GetAttachParentActor())
	{
		OtherActor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform, TEXT("hand_r"));
		OtherActor->SetActorRelativeLocation({ 0,0,0 });
		OtherActor->SetActorHiddenInGame(true);
		SetActorScale3D({ 1.3f, 1.3f,1.3f });
		SetActorTickEnabled(true);
		AttackingCounter = AttackingTimer;

		if (bCanAttack)
		{
			return;
		}

		bCanAttack = true;
		OnPowerUpStart.ExecuteIfBound();
		UE_LOG(LogAIBallCatchCharacter, Warning, TEXT("Catched a ball! Power up enabled!"));
	}
	else if (OtherActor->ActorHasTag(TEXT("Enemy")) && bCanAttack)
	{
		APawn* OtherPawn = Cast<APawn>(OtherActor);
		if (OtherPawn)
		{
			if (OtherPawn->GetController()->Implements<UBallGameInterface>())
			{
				IBallGameInterface* StunActor = Cast<IBallGameInterface>(OtherPawn->GetController());
				const bool bWasAlreadyStunned = StunActor->Execute_Stun(OtherPawn->GetController());
				if (!bWasAlreadyStunned)
				{
					OnStunEnemy.ExecuteIfBound();
				}
			}
		}


	}
}