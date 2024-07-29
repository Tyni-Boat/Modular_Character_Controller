// Copyright � 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "ComponentAndBase/Enums.h"
#include "Engine/HitResult.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Components/PrimitiveComponent.h"
#include "CommonTypes.generated.h"


#pragma region Inputs


//Input entry structure. InputEntryNature_Axis X should be used for value types.
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FInputEntry
{
	GENERATED_BODY()

public:
	FInputEntry();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	EInputEntryNature Nature = EInputEntryNature::Button;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	EInputEntryType Type = EInputEntryType::Simple;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	FVector Axis = FVector(0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	float InputBuffer = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	EInputEntryPhase Phase = EInputEntryPhase::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	float HeldDuration = 0;


	void Reset();
};


//Represent a pack of input entry, tracking inputs. Used locally only. not intended to be used remotely
UCLASS(BlueprintType)
class MODULARCONTROLLER_API UInputEntryPool : public UObject
{
	GENERATED_BODY()

public:
	//The input pool
	TMap<FName, FInputEntry> _inputPool;

	//The input pool of the last frame
	TMap<FName, FInputEntry> _inputPool_last;


	// Add input to the input pool. return true when added not replaced
	bool AddOrReplace(FName key, FInputEntry entry, const bool hold = false);

	// Get input from the inputs pool
	FInputEntry ReadInput(const FName key, bool consume = false);

	// Update the inputs pool
	void UpdateInputs(float delta, const bool debug = false, UObject* worldContext = NULL);
};


#pragma endregion


#pragma region Surface and Zones

// An extension of HitResult containing the Hit response
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FHitResultExpanded
{
	GENERATED_BODY()

public:
	FHitResultExpanded();

	FHitResultExpanded(FHitResult hit, int index = 0, ECollisionResponse queryType = ECR_MAX);

	// Hit result
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Expansion")
	FHitResult HitResult;

	// The hit index of how many time the same component is hit
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Expansion")
	int HitIndex = 0;

	// The query response.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Expansion")
	TEnumAsByte<ECollisionResponse> QueryResponse = ECollisionResponse::ECR_MAX;

	// The hit object's type.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Expansion")
	TEnumAsByte<EObjectTypeQuery> ObjectType = ObjectTypeQuery_MAX;
};


// Represent a surface and it's movements
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FSurface
{
	GENERATED_BODY()

public:
	FSurface();

	FSurface(FHitResultExpanded hit, bool canStepOn = true);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Tracking")
	TWeakObjectPtr<UPrimitiveComponent> TrackedComponent = NULL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Tracking")
	FName TrackedComponentBoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Tracking")
	int TrackedComponentIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FVector SurfacePoint = FVector(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FVector SurfaceNormal = FVector(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FVector SurfaceImpactNormal = FVector(0);

	// Get surface friction (X), surface bounciness (Y), Hit collision Response type (Z) as ECollisionResponse and Can Character StepOn (W) as boolean
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FVector4f SurfacePhysicProperties = FVector4f(1, 0, 0, 1);


	// Update the tacking of the component.
	bool UpdateTracking(float deltaTime);

	// Update information about the hit
	void UpdateHit(FHitResultExpanded hit, bool canStepOn = true);

	// Apply a force on the surface at a point on it and return the velocity of the surface at the point before force application. use reaction to apply force only if it's opposed to the surface normal
	FVector ApplyForceAtOnSurface(const FVector point, const FVector force, bool reactionForce = false) const;

	// Get the velocity planed on the surface normal. reaction planar return the same velocity if the dot product with normal > 0.
	FVector GetVelocityAlongNormal(const FVector velocity, const bool useImpactNormal = false, const bool reactionPlanarOnly = false) const;

	// Get the velocity at a point on the surface. in cm/sec
	FVector GetVelocityAt(const FVector point, const float deltaTime = 0) const;


	//Linear Velocity
public:
	//The linear velocity in Cm/s
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Tracking")
	FVector LinearVelocity = FVector(0);

private:
	FVector _lastPosition = FVector(NAN);

	//Angular Velocity
public:
	// The angular velocity (axis * angle) in Deg/s
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Tracking")
	FVector AngularVelocity = FVector(0);

private:
	FQuat _lastRotation = FQuat(NAN,NAN,NAN,NAN);
};


// Represent some of the common parameters used when testing a surface.
USTRUCT(BlueprintType)
struct FSurfaceCheckParams
{
	GENERATED_BODY()

public:
	FSurfaceCheckParams();

	// The prediction along the trajectory range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D PredictionDistanceRange = FVector2D(-1);

	// The range of the surface point's distance along the checking direction. (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D HeightRange = FVector2D(-1);

	// The range of depths of the surface (X: Min step depth, Y: Min Vault fall height, Z: MaxDepthCheckDistance) (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector DepthRange = FVector(-1, -1, 1);

	// The range of normalized distances of the surface hit point compared to the closest point on controller's primitive (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D OffsetRange = FVector2D(-1);

	// The range of the velocity along the checking direction (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D SpeedRange = FVector2D(-1);

	// The range of the velocity in the surface direction (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D OrientationSpeedRange = FVector2D(-1);

	// The range of the surface velocity along the checking direction at impact point (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D SurfaceSpeedRange = FVector2D(-1);

	// The angle range from normal (degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D NormalAngleRange = FVector2D(-1);

	// The angle range from impact normal (degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D ImpactAngleRange = FVector2D(-1);

	// The angle range from orientation vector (degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector2D OrientationAngleRange = FVector2D(-1);

	// Additional cosmetic variables ranges
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	TMap<FName, FVector2D> CosmeticVarRanges;

	//The collision response of the surface
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	TEnumAsByte<ECollisionResponse> CollisionResponse = ECR_Block;

	//The surface can be stept on?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	bool bMustBeStepable = true;
};


// Response when checking surface params
USTRUCT(BlueprintType)
struct FSurfaceCheckResponse
{
	GENERATED_BODY()

	FSurfaceCheckResponse();

	// The surface that was test aginst
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FSurface Surface = FSurface();

	// The vault depth vector (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Params")
	FVector VaultDepthVector = FVector(NAN);
};

#pragma endregion


#pragma region States and Actions

// Keep live infos about an action
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FActionInfos
{
	GENERATED_BODY()

public:
	FActionInfos();

	// The action cooldown timer
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Infos")
	double _cooldownTimer = 0;

	// The remaining total action timer
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Infos")
	double _remainingActivationTimer = 0;

	// The Number of repeats in a row
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Infos")
	int _repeatCount = 0;

	// The Phases Durations. X-Anticipation, Y-Active, Z-Recovery
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Infos")
	FVector _startingDurations = FVector(0);

	// The montage index in the montage library.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Infos")
	int _montageLibraryIndex = -1;

	// The current action phase
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Infos")
	EActionPhase CurrentPhase = EActionPhase::Undetermined;


	void Init(FVector timings, float coolDown, int repeatCount = 0, int montageIndex = -1);

	double GetRemainingActivationTime() const;

	double GetRemainingCoolDownTime() const;

	double GetNormalizedTime(EActionPhase phase) const;

	double GetPhaseRemainingTime(EActionPhase phase) const;
	
	double GetPhaseElapsedTime(EActionPhase phase) const;

	void SkipTimeToPhase(EActionPhase phase);

	void Update(float deltaTime, bool allowCooldownDecrease = true);

	void Reset(float coolDown = 0);
};


// Represent an action montage parameter
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FActionMotionMontage
{
	GENERATED_BODY()

public:
	FActionMotionMontage();

	/// <summary>
	/// The Animation Montages to play
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	//TSoftObjectPtr<UAnimMontage> Montage;
	UAnimMontage* Montage = nullptr;

	/// <summary>
	/// The Animation Montages section to play
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	FName MontageSection = NAME_None;

	/// <summary>
	/// Use this montage lenght for action duration?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bUseMontageLenght = false;

	/// <summary>
	/// Use this montage Sections for action phase repartition?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bUseMontageSectionsAsPhases = false;

	/// <summary>
	/// Play montage on the state's linked anim blueprint?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bPlayOnState = false;

	/// <summary>
	/// Stop montage with action end
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bStopOnActionEnds = false;
};


// Store an array of action montages
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FActionMontageLibrary
{
	GENERATED_BODY()

public:
	FActionMontageLibrary();

	// Motion library array
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	TArray<FActionMotionMontage> Library;

	// Override The Animation Montages section to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	FName OverrideMontageSection = NAME_None;

	// Override Use this montage lenght for action duration?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bOverrideUseMontageLenght = false;

	// Override Use this montage Sections for action phase repartition?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bOverrideUseMontageSectionsAsPhases = false;

	/// <summary>
	/// Override Play montage on the state's linked anim blueprint?
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bOverridePlayOnState = false;

	/// <summary>
	/// Override Stop montage with action end
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Types|Montage")
	bool bOverrideStopOnActionEnds = false;
};


// The infos about the state and actions of the controller
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FStatusParameters
{
	GENERATED_BODY()

public:
	FStatusParameters();

	bool HasChanged(FStatusParameters otherStatus) const;

	//The Index of the State used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	int StateIndex = -1;

	//The Index of the Action used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	int ActionIndex = -1;

	//The primary flag of the state used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	int PrimaryStateFlag = 0;

	//The primary action action flag, used to know if an action just being repeated (1 or 0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	int PrimaryActionFlag = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	FVector StateModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	FVector ActionsModifiers;

	// Cosmetic states and actions variables. useful for let say know the distance from the ground while airborne. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	TMap<FName, float> StatusCosmeticVariables;
};


#pragma endregion


#pragma region MovementInfosAndReplication


//Represent a single kinematic linear condition
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FLinearKinematicCondition
{
	GENERATED_BODY()

public:
	FLinearKinematicCondition();

	FLinearKinematicCondition(FVector position, FVector velocity = FVector(0), FVector acceleration = FVector(0));

	//The linear acceleration (Cm/s2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	FVector Acceleration = FVector(0);

	//The linear velocity (Cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	FVector Velocity = FVector(0);

	//The position (Cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	FVector Position = FVector(0);

	//The current velocity of the referential space (usually the surface the controller is on).Is conserved. this is not mean to be used directly.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KinematicProperty", SkipSerialization)
	FVector refVelocity = FVector(0);

	//The current Acceleration caused by the referential space (usually the surface the controller is on).Is not conserved. this is not mean to be used directly.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KinematicProperty", SkipSerialization)
	FVector refAcceleration = FVector(0);

	//Vector used to adjust position without conserving the movement. (Cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty", SkipSerialization)
	FVector SnapDisplacement = FVector(0);

	//The array of composite movements. this is not mean to be used directly.
	UPROPERTY(SkipSerialization)
	TArray<FVector4d> CompositeMovements;

	//The time elapsed (s)
	UPROPERTY(SkipSerialization, EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	double Time = 0;


public:
	//Evaluate future movement conditions base on the delta time.
	FLinearKinematicCondition GetFinalCondition(double deltaTime);

	//Evaluate future movement conditions base both the delta time and a targeted position.
	FLinearKinematicCondition GetFinalFromPosition(FVector targetPosition, double deltaTime, bool affectAcceleration = false);

protected:
	//Compute composite movement to take them in to account when moving.
	void ComputeCompositeMovement(const float delta);
};


//Represent a single kinematic angular condition
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FAngularKinematicCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	FVector AngularAcceleration = FVector(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	FVector RotationSpeed = FVector(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	FQuat Orientation = FQuat(0);

	UPROPERTY(SkipSerialization, EditAnywhere, BlueprintReadWrite, Category = "KinematicProperty")
	double Time = 0;


	//Get angular speed as Quaternion
	FQuat GetAngularSpeedQuat(float time = 1) const;

	//Evaluate future movement conditions base on the delta time.
	FAngularKinematicCondition GetFinalCondition(double deltaTime, FVector* targetLookDir = nullptr, FVector* rotateVector = nullptr, FQuat* rotDiff = nullptr) const;
};


//Represent the kinematic conditions of an object
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FKinematicComponents
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FLinearKinematicCondition LinearKinematic = FLinearKinematicCondition();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FAngularKinematicCondition AngularKinematic = FAngularKinematicCondition();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FVector Gravity = FVector::DownVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurfaceHandling")
	int SurfaceBinaryFlag = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceHandling")
	TArray<FSurface> SurfacesInContact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceHandling")
	FHitResultExpanded LastMoveHit;


	FKinematicComponents();

	FKinematicComponents(FLinearKinematicCondition linearCond, FAngularKinematicCondition angularCond, TArray<FSurface>* surfaces = nullptr, int surfacesActive = -1);

	// Make an action for each surface from the set of surfaces in contact defined by SurfaceBinaryFlag
	bool ForEachSurface(std::function<void(FSurface)> doAction, bool onlyValidOnes = true) const;

	//Get the rotation from angular kinematic.
	FQuat GetRotation() const;

	FORCEINLINE FVector GetGravity() const { return Gravity; }
	FORCEINLINE FVector GetGravityDirection() const { return Gravity.GetSafeNormal(); }
	FORCEINLINE float GetGravityScale() const { return Gravity.Length(); }
};


//Represent a kinematic prediction's point
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FKinematicPredictionSample
{
	GENERATED_BODY()

public:
	FKinematicPredictionSample();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FLinearKinematicCondition LinearKinematic = FLinearKinematicCondition();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FAngularKinematicCondition AngularKinematic = FAngularKinematicCondition();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FVector Gravity = FVector::DownVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time")
	float RelativeTime = -1;
};


//The Status of the controller including Kinematic and Status Parameters
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FControllerStatus
{
	GENERATED_BODY()

public:
	// Movement status
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FKinematicComponents Kinematics = FKinematicComponents();

	// States and Action status
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FStatusParameters StatusParams = FStatusParameters();

	// User movement, as it came from the input pool
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FVector MoveInput = FVector(0);

	// The custom direction to find colliding surfaces. Use W to limit the impact point height detection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FVector4 CustomSolverCheckParameters = FVector4(0);

	// The custom zone drag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	float CustomPhysicDrag = -1;

	// The time offset from the current time. negative values are past and positive future. zero is present.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	float TimeOffset = 0;
};


// The result of a check on state or action
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FControllerCheckResult
{
	GENERATED_BODY()

public:
	FORCEINLINE FControllerCheckResult()
	{
	}

	FORCEINLINE FControllerCheckResult(bool condition, FControllerStatus process)
	{
		CheckedCondition = condition;
		ProcessResult = process;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckResult")
	bool CheckedCondition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckResult")
	FControllerStatus ProcessResult = FControllerStatus();
};


// Represent an override root motion command
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FOverrideRootMotionCommand
{
	GENERATED_BODY()

public:
	FORCEINLINE FOverrideRootMotionCommand()
	{
		Reset();
	}

	FORCEINLINE FOverrideRootMotionCommand(FOverrideRootMotionCommand& Other)
	{
		Reset();
		OverrideRotationRootMotionMode = Other.OverrideRotationRootMotionMode;
		OverrideTranslationRootMotionMode = Other.OverrideTranslationRootMotionMode;
		Duration = Other.Duration;
		PlayRate = Other.PlayRate;
		WarpKey = Other.WarpKey;
		WarpCurve = Other.WarpCurve;
		WarpTransform_Path = Other.WarpTransform_Path;
	}

	FORCEINLINE bool IsValid() const
	{
		return OverrideRotationRootMotionMode != ERootMotionType::NoRootMotion || OverrideTranslationRootMotionMode != ERootMotionType::NoRootMotion;
	}

	FORCEINLINE bool IsMotionWarpingEnabled() const
	{
		return WarpKey != NAME_None && WarpTransform_Path.Num() > 0;
	}

	FORCEINLINE void Reset()
	{
		OverrideRotationRootMotionMode = ERootMotionType::NoRootMotion;
		OverrideTranslationRootMotionMode = ERootMotionType::NoRootMotion;
		Duration = 0;
		Time = 0;
		PlayRate = 1;
		WarpKey = NAME_None;
		WarpTransform_Path.Empty();
	}

	FORCEINLINE bool Update(float deltaTime, FTransform& warpTransform, std::function<void()> OnReset)
	{
		if (!IsValid())
			return false;
		Time += deltaTime * PlayRate;
		const float normalizedTime = FMath::GetMappedRangeValueClamped(TRange<float>(0, Duration), TRange<float>(0, 1), Time);
		if (WarpKey != NAME_None)
		{
			const float alpha = FAlphaBlend::AlphaToBlendOption(normalizedTime, WarpCurve);
			const float cursor = alpha * (WarpTransform_Path.Num() - 1);
			const int index = FMath::Clamp(static_cast<int>(cursor), 0, WarpTransform_Path.Num() - 2);
			const float interIndexAlpha = FMath::Clamp(cursor - index, 0, 1);
			const int nextIndex = index + 1;
			if(WarpTransform_Path.IsValidIndex(index) && WarpTransform_Path.IsValidIndex(nextIndex))
			{
				const FTransform matchTransform = FTransform(FQuat::Slerp(WarpTransform_Path[index].GetRotation(), WarpTransform_Path[nextIndex].GetRotation(), interIndexAlpha),
															 FMath::Lerp(WarpTransform_Path[index].GetLocation(), WarpTransform_Path[nextIndex].GetLocation(), interIndexAlpha),
															 FVector(1));
				warpTransform = matchTransform;
			}
		}
		if (Time >= Duration)
		{
			OnReset();
			Reset();
			return false;
		}
		return true;
	}

	//The override translation rootMotion mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Mode")
	ERootMotionType OverrideTranslationRootMotionMode = ERootMotionType::NoRootMotion;

	//The override rotation rootMotion mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Mode")
	ERootMotionType OverrideRotationRootMotionMode = ERootMotionType::NoRootMotion;

	// The warp key name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Warp")
	FName WarpKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Warp")
	EAlphaBlendOption WarpCurve;

	// The Warp transform path
	UPROPERTY()
	TArray<FTransform> WarpTransform_Path;

	// The animation play rate
	UPROPERTY()
	float PlayRate = 0;

	// The duration of the command
	UPROPERTY()
	float Duration = 0;

	// The current command eval time.
	UPROPERTY()
	float Time = 0;
};


#pragma endregion


#pragma region Network and Replication

//Net Data from a kinematic movement
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FNetKinematic
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	FVector_NetQuantizeNormal MoveInput = FVector(0);

	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	FVector_NetQuantize Velocity = FVector(0);

	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	FVector_NetQuantize Position = FVector(0);

	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	FVector_NetQuantizeNormal Orientation = FVector(0);


	//Extract Net kinematics from Controller status
	void ExtractFromStatus(FControllerStatus status);

	// Restore Net Kinematics onto the controller status
	void RestoreOnToStatus(FControllerStatus& status) const;
};


//Net Data from  status params
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FNetStatusParam
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	int StateIndex = 0;

	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	int ActionIndex = 0;

	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	int StateFlag = 0;

	UPROPERTY(VisibleAnywhere, Category = "Network Kinematics")
	int ActionFlag = 0;


	//Extract Net Status from Controller status
	void ExtractFromStatus(FControllerStatus status);

	// Restore Net Status onto the controller status
	void RestoreOnToStatus(FControllerStatus& status) const;
};

#pragma endregion


#pragma region Animation


#pragma endregion
