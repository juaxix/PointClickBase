// xixgames - juaxix - 2024

#include "PointClickPawn.h"

#include <Camera/CameraComponent.h>
#include <Components/InputComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/Engine.h>
#include <Engine/StaticMesh.h>
#include <GameFramework/SpringArmComponent.h>

TAutoConsoleVariable<float> APointClickPawn::CVarDesiredScreenCoverage(
	TEXT("PointClick.DesiredScreenCoverage"),
	0.23f,
	TEXT("Desired screen coverage for viewed objects (0.0 to 1.0)"),
	ECVF_Default);

TAutoConsoleVariable<float> APointClickPawn::CVarMinViewZoomFactor(
	TEXT("PointClick.MinViewZoomFactor"),
	0.1f,
	TEXT("Min View Zoom Factor while viewing an object"),
	ECVF_Default);

TAutoConsoleVariable<float> APointClickPawn::CVarMaxViewZoomFactor(
	TEXT("PointClick.MaxViewZoomFactor"),
	5.f,
	TEXT("Max View Zoom Factor while viewing an object"),
	ECVF_Default);

TAutoConsoleVariable<float> APointClickPawn::CVarZoomSpeedFactor(
	TEXT("PointClick.ZoomSpeedFactor"),
	0.1f,
	TEXT("Zoom Factor de/increment speed"),
	ECVF_Default);

APointClickPawn::APointClickPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// Create the root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Create a camera boom (spring)
	CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CamSpringArm"));
	CameraSpringArm->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	CameraSpringArm->TargetArmLength = ZoomedInDistance;
	CameraSpringArm->SetRelativeRotation(FRotator(-80.f, 0.f, 0.f));
	CameraSpringArm->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level
	CameraSpringArm->bEnableCameraLag = true;
	CameraSpringArm->CameraLagSpeed = 3.f;

	// Create a camera...
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	CameraComponent->AttachToComponent(CameraSpringArm, FAttachmentTransformRules::KeepRelativeTransform,
									   USpringArmComponent::SocketName);

	// Create the spawning preview mesh
	ViewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ViewMesh"));
	ViewMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ViewMeshComponent->SetCollisionObjectType(ECC_Destructible);
	
	// Ignored by VisionSphere for other colliders in the game
	ViewMeshComponent->SetUsingAbsoluteLocation(true);
	ViewMeshComponent->SetUsingAbsoluteRotation(true);
}

void APointClickPawn::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* const PC = GetController<APlayerController>();
	if (IsValid(PC))
	{
		PC->SetInputMode(FInputModeGameAndUI()
			.SetHideCursorDuringCapture(false)
			.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
		PC->SetShowMouseCursor(true);
	}
}

void APointClickPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	switch (CurrentClickMode)
	{
	case EClickMode::View:
		LerpObjectToCamera(DeltaTime);
		break;
	case EClickMode::Selecting:
		MovePawnWithCamera(DeltaTime);
		break;
	}
}

void APointClickPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);

	// set up key bindings
	PlayerInputComponent->BindAxis(TEXT("MoveForward"));
	PlayerInputComponent->BindAxis(TEXT("MoveRight"));
	PlayerInputComponent->BindAction(TEXT("ZoomIn"), IE_Pressed, this, &ThisClass::ZoomIn);
	PlayerInputComponent->BindAction(TEXT("ZoomOut"), IE_Pressed, this, &ThisClass::ZoomOut);
	PlayerInputComponent->BindAction(TEXT("ClickSelection"), IE_Pressed, this, &ThisClass::ClickPressed);
	PlayerInputComponent->BindAction(TEXT("ClickRelease"), IE_Pressed, this, &ThisClass::ChangeClickMode<EClickMode::Selecting>);
}

void APointClickPawn::ZoomIn()
{
	if (CurrentClickMode == EClickMode::Selecting)
	{
		CurrentZoomSpeed = -ZoomedInMoveSpeed * CVarZoomSpeedFactor.GetValueOnAnyThread();
		TargetZoomFactor = FMath::Max(ZoomFactor - 0.1f, 0.f); // Zoom in by 10%
	}
	else // View mode
	{
		CurrentZoomSpeed = ZoomedInMoveSpeed * CVarZoomSpeedFactor.GetValueOnAnyThread();
		TargetZoomFactor = FMath::Min(ViewModeZoomFactor + 0.1f, CVarMaxViewZoomFactor.GetValueOnAnyThread());
	}
}

void APointClickPawn::ZoomOut()
{
	if (CurrentClickMode == EClickMode::Selecting)
	{
		CurrentZoomSpeed = ZoomedOutMoveSpeed * CVarZoomSpeedFactor.GetValueOnAnyThread();
		TargetZoomFactor = FMath::Min(ZoomFactor + 0.1f, 1.f); // Zoom out by 10%
	}
	else // View mode
	{
		CurrentZoomSpeed = -ZoomedOutMoveSpeed * CVarZoomSpeedFactor.GetValueOnAnyThread();
		TargetZoomFactor = FMath::Max(ViewModeZoomFactor - 0.1f, CVarMinViewZoomFactor.GetValueOnAnyThread());
	}
}

void APointClickPawn::ClickPressed()
{
	switch (CurrentClickMode)
	{
	case EClickMode::Selecting:
		{
			FindActorInView();
			break;
		}
	default: return;
	}
}

void APointClickPawn::FindActorInView()
{
	const UWorld* const World = GetWorld();
	const APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	PlayerController->DeprojectMousePositionToWorld(MouseLocation, MouseDirection);
	const FVector EndLocation = MouseLocation - (MouseDirection * (MouseLocation.Z / MouseDirection.Z) * MAX_CLICK_DISTANCE);
	TArray<FHitResult> HitResults;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(this);
	DrawDebugLine(World, MouseLocation, EndLocation, FColor::Purple, false, 15.f, 1, 0.21f);
	if (World->LineTraceMultiByChannel(HitResults, MouseLocation, EndLocation, ECollisionChannel::ECC_WorldDynamic, CollisionQueryParams))
	{
		for (FHitResult& HitResult: HitResults)
		{
			AActor* HitActor = HitResult.GetActor();
			UStaticMeshComponent* HitMesh = IsValid(HitActor) && HitActor->Tags.Contains(ClickableName)
				? HitActor->GetComponentByClass<UStaticMeshComponent>() : nullptr;
			if (IsValid(HitMesh) && HitMesh->GetStaticMesh())
			{
				DrawDebugCone(World, HitResult.Location, FVector::UpVector, 300.f, 35.f, 35.f, 12, FColor::Cyan, false, 2.f);
				ViewMeshComponent->SetStaticMesh(HitMesh->GetStaticMesh());
				ChangeClickMode<EClickMode::View>();
				return;
			}
		}
	}
}

void APointClickPawn::MovePawnWithCamera(float DeltaTime)
{
    // Find movement direction
    const float ForwardValue = GetInputAxisValue(TEXT("MoveForward"));
    const float RightValue = GetInputAxisValue(TEXT("MoveRight"));

    // Clamp max size so that (X=1, Y=1) doesn't cause faster movement in diagonal directions
    FVector MoveDirection = FVector(ForwardValue, RightValue, 0.f).GetClampedToMaxSize(1.f);

    // If non-zero size, move this actor
    if (!MoveDirection.IsZero())
    {
        const float MoveSpeed = FMath::Lerp(ZoomedInMoveSpeed, ZoomedOutMoveSpeed, ZoomFactor);

        MoveDirection = MoveDirection.GetSafeNormal() * MoveSpeed * DeltaTime;

        FVector NewLocation = GetActorLocation();
        NewLocation += GetActorForwardVector() * MoveDirection.X;
        NewLocation += GetActorRightVector() * MoveDirection.Y;

        SetActorLocation(NewLocation);
    }

	// Handle zooming
	if (CurrentZoomSpeed != 0.f)
	{
		ZoomFactor = FMath::FInterpTo(ZoomFactor, TargetZoomFactor, DeltaTime, 5.0f);
        
		if (FMath::IsNearlyEqual(ZoomFactor, TargetZoomFactor, 0.01f))
		{
			ZoomFactor = TargetZoomFactor;
			CurrentZoomSpeed = 0.f;
		}
	}

	// Smoothly interpolate the camera spring arm length
	float TargetArmLength = FMath::Lerp(ZoomedInDistance, ZoomedOutDistance, ZoomFactor);
	constexpr float InterpToSpeed = 6.f; 
	CameraSpringArm->TargetArmLength = FMath::FInterpTo(CameraSpringArm->TargetArmLength, TargetArmLength, DeltaTime, InterpToSpeed);
}

void APointClickPawn::LerpObjectToCamera(float DeltaTime)
{
	if (CurrentClickMode == EClickMode::View && ViewMeshComponent->GetStaticMesh())
	{
		APlayerController* PlayerController = Cast<APlayerController>(GetController());
		if (!IsValid(PlayerController))
		{
			return;
		}

		// Handle zooming in View mode
		if (CurrentZoomSpeed != 0.0f)
		{
			ViewModeZoomFactor = FMath::FInterpTo(ViewModeZoomFactor, TargetZoomFactor, DeltaTime, 5.0f);
            
			if (FMath::IsNearlyEqual(ViewModeZoomFactor, TargetZoomFactor, 0.01f))
			{
				ViewModeZoomFactor = TargetZoomFactor;
				CurrentZoomSpeed = 0.0f;
			}
		}

		const FVector& CameraForward = CameraComponent->GetForwardVector();
		int32 ViewportSizeX, ViewportSizeY;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		// Scale DesiredScreenCoverage based on ViewModeZoomFactor
		const float BaseDesiredScreenCoverage = FMath::Clamp(CVarDesiredScreenCoverage.GetValueOnGameThread(), 0.1f, 1.0f);
		const float DesiredScreenCoverage = BaseDesiredScreenCoverage * ViewModeZoomFactor;
		const float ScreenSize = FMath::Min(ViewportSizeX, ViewportSizeY) * DesiredScreenCoverage;
		const float ObjectSize = ViewMeshComponent->GetStaticMesh()->GetBounds().BoxExtent.GetAbsMax() * 2; // Diameter of the object

		// Calculate the distance to keep the object at the desired screen size
		const float FOV = CameraComponent->FieldOfView;
		const float Distance = (ObjectSize * 0.5f) / FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) * (ViewportSizeY / ScreenSize);

		// Calculate the target position
		const FVector& CameraLocation = CameraComponent->GetComponentLocation();
		const FVector TargetPosition = CameraLocation + CameraForward * Distance;

		// Smoothly interpolate to the target position
		const FVector& CurrentPosition = ViewMeshComponent->GetComponentLocation();
		const FVector NewPosition = FMath::VInterpTo(CurrentPosition, TargetPosition, DeltaTime, 5.0f);

		// Update the position
		ViewMeshComponent->SetWorldLocation(NewPosition);

		// Rotate the object based on movement
		FRotator NewRotation = ViewMeshComponent->GetComponentRotation();

		// Rotation based on MoveForward and MoveRight
		const float ForwardValue = GetInputAxisValue(TEXT("MoveForward"));
		const float RightValue = GetInputAxisValue(TEXT("MoveRight"));
		NewRotation.Yaw += RightValue * RotationSpeed * DeltaTime;
		NewRotation.Pitch += ForwardValue * RotationSpeed * DeltaTime;

		// Apply the new rotation
		ViewMeshComponent->SetWorldRotation(NewRotation);
	}
}

FVector APointClickPawn::GetCursorPositionInActionLayer()
{
	const UWorld* const World = GetWorld();
	const APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	PlayerController->DeprojectMousePositionToWorld(MouseLocation, MouseDirection);
	const FVector EndLocation = MouseLocation - (MouseDirection * (MouseLocation.Z / MouseDirection.Z) * MAX_CLICK_DISTANCE);
	FHitResult HitResult;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(this);
	DrawDebugLine(World, MouseLocation, EndLocation, FColor::Purple, false, 15.f, 1, 0.21f);
	if (World->LineTraceSingleByChannel(HitResult, MouseLocation, EndLocation, ECollisionChannel::ECC_Visibility, CollisionQueryParams))
	{
		DrawDebugCone(World, HitResult.Location, FVector::UpVector, 300.f, 35.f, 35.f, 12, FColor::Cyan, false, 2.f);
		return HitResult.Location;
	}
	
	return FVector::Zero();
}

template <EClickMode ClickMode>
void APointClickPawn::ChangeClickMode()
{
	if (CurrentClickMode != ClickMode)
	{
		CurrentClickMode = ClickMode;
		if (ClickMode == EClickMode::Selecting)
		{
			ViewMeshComponent->SetStaticMesh(nullptr);
		}
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
			FString::Printf(TEXT("ClickMode: %d"), static_cast<int32>(ClickMode)));
	}
}
