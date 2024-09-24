// xixgames - juaxix - 2024

#pragma once

#include <GameFramework/Pawn.h>

#include "PointClickPawn.generated.h"

UENUM(BlueprintType)
enum class EClickMode : uint8
{
	Selecting,
	View
};

UCLASS()
class POINTCLICKBASE_API APointClickPawn : public APawn
{
	GENERATED_BODY()
public:
	APointClickPawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
		
	const FName ClickableName{FName(TEXT("Clickable"))};
	const float MAX_CLICK_DISTANCE = 100000.0f;
protected:
	void ZoomIn();
	void ZoomOut();
	void ClickPressed();
	void FindActorInView();
	void MovePawnWithCamera(float DeltaTime);
	void LerpObjectToCamera(float DeltaTime);
	
	template <EClickMode ClickMode>
	void ChangeClickMode();
	
	/* Returns the cursor position inside the game action layer */
	FVector GetCursorPositionInActionLayer();
	
	/* The speed the camera moves around the level, when zoomed out */
	UPROPERTY(Category = Camera, EditAnywhere, BlueprintReadWrite)
	float ZoomedOutMoveSpeed = 3500.f;

	/* The speed the camera moves around the level, when zoomed in */
	UPROPERTY(Category = Camera, EditAnywhere, BlueprintReadWrite)
	float ZoomedInMoveSpeed = 1750.f;

	/* The distance the camera is from the action layer, when zoomed out */
	UPROPERTY(Category = Camera, EditAnywhere, BlueprintReadWrite)
	float ZoomedOutDistance = 3000.f;

	/* The distance the camera is from the action layer, when zoomed in */
	UPROPERTY(Category = Camera, EditAnywhere, BlueprintReadWrite)
	float ZoomedInDistance = 1400.f;

	/* The camera */
	UPROPERTY(Category = Camera, VisibleAnywhere, BlueprintReadOnly)
	class UCameraComponent* CameraComponent = nullptr;

	/* Camera boom positioning the camera above the character */
	UPROPERTY(Category = Camera, VisibleAnywhere, BlueprintReadOnly)
	class USpringArmComponent* CameraSpringArm = nullptr;

	/* The mesh component to select as preview */
	UPROPERTY(Category = Spawn, EditDefaultsOnly)
	class UStaticMeshComponent* ViewMeshComponent;
	
	UPROPERTY(Category = Camera, VisibleAnywhere, BlueprintReadOnly)
	EClickMode CurrentClickMode = EClickMode::Selecting;
	
	UPROPERTY(Category = Camera, VisibleAnywhere, BlueprintReadOnly)
	float RotationSpeed = 100.f;
	
	float ZoomFactor = 0.f;
	FVector MouseLocation = FVector::ZeroVector;
	FVector MouseDirection = FVector::ForwardVector;
	float CurrentZoomSpeed = 0.f;
	float TargetZoomFactor = 0.f;
	float ViewModeZoomFactor = 0.5f; // Initial value for View mode
	static TAutoConsoleVariable<float> CVarMinViewZoomFactor;
	static TAutoConsoleVariable<float> CVarMaxViewZoomFactor;
	static TAutoConsoleVariable<float> CVarZoomSpeedFactor;
	static TAutoConsoleVariable<float> CVarDesiredScreenCoverage;
};

