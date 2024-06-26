// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once

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
	TEnumAsByte<EInputEntryNature> Nature = EInputEntryNature::InputEntryNature_Button;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	EInputEntryType Type = EInputEntryType::Simple;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	FVector Axis = FVector(0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	float InputBuffer = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	TEnumAsByte<EInputEntryPhase> Phase = EInputEntryPhase::InputEntryPhase_None;

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


// State Behavior ability to track surface velocity. Intended to be used in local
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FSurfaceInfos
{
	GENERATED_BODY()

public:

	FSurfaceInfos();

	//Update surface from a hit result and the transform that hit the surface.
	void UpdateSurfaceInfos(FTransform inTransform, const FHitResult selectedSurface, const float delta);

	// Release the update lock
	void ReleaseLock();

	// Reset the surface infos
	void Reset();

	// Consume the last evaluated linear velocity
	FVector ConsumeSurfaceLinearVelocity(bool linear = true, bool angular = true, bool centripetal = false);

	// Get the last evaluated linear velocity (cm/s)
	FVector GetSurfaceLinearVelocity(bool linear = true, bool angular = true, bool centripetal = false) const;

	// Get the last evaluated angular velocity
	FQuat GetSurfaceAngularVelocity(bool consume = false);

	// Get the last evaluated surface normal
	FVector GetSurfaceNormal() const;

	// Get surface primitive
	UPrimitiveComponent* GetSurfacePrimitive() const;

	// Get last surface primitive
	UPrimitiveComponent* GetLastSurfacePrimitive() const;

	// Get surface hit result data
	FHitResult GetHitResult() const;

	// Get if the surface was changed
	bool HadChangedSurface() const;

	// Get if the surface we just landed on this surface
	bool HadLandedOnSurface() const;

	// Get if the surface we just took off this surface
	bool HadTookOffSurface() const;

public:

	//the surface hit raycast
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FHitResult _surfaceHitResult;

protected:
	//------------------------------------------------------------------------------------------

	//the current surface
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	TWeakObjectPtr<UPrimitiveComponent> _currentSurface;

	//the last surface
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	TWeakObjectPtr<UPrimitiveComponent> _lastSurface;

	//the surface linear velocity
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceLinearCompositeVelocity = FVector(0);

	//the surface angular velocity
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceAngularCompositeVelocity = FVector(0);

	//the surface angular centripedal velocity
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceAngularCentripetalVelocity = FVector(0);

	//the surface normal
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceNormal = FVector(0);

	//the surface angular velocity
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FQuat _surfaceAngularVelocity = FQuat(0);

	//the surface velocity
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceLocalHitPoint = FVector(0);

	//the surface velocity
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceLocalLookDir = FVector(0);

	//the absolute location of the platform during last frame
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _currentSurface_Location = FVector(INFINITY);

	//The absolute rotation of the platform during last frame
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FQuat _currentSurface_Rotation = FQuat::Identity;

	//prevent the surface from being updated twice during a frame.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	bool updateLock = false;

	//Enabled if the surface had been switched during this frame.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	bool isSurfaceSwitch = false;
};


// Used to track a surface movements
USTRUCT(BlueprintType)
struct FSurfaceTrackData
{
	GENERATED_BODY()

public:

	FSurfaceTrackData();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Tracking")
	TWeakObjectPtr<UPrimitiveComponent> TrackedComponent = NULL;


	// Update the tacking of the component.
	bool UpdateTracking(float deltaTime);

	// Get the velocity at a point on the surface.
	FVector GetVelocityAt(const FVector point) const;


	//Linear Velocity
public:

	//The linear velocity in Cm/s
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Tracking")
	FVector LinearVelocity = FVector(0);

private:

	FVector _lastPosition = FVector(0);

	//Angular Velocity
public:

	// The angular velocity (axis * angle) in Rad/s
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Tracking")
	FVector AngularVelocity = FVector(0);

private:

	FQuat _lastRotation = FQuat::Identity;

};


// The informations about a surface.
USTRUCT(BlueprintType)
struct FSurface
{
	GENERATED_BODY()

public:

	FSurface();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	TWeakObjectPtr<UPrimitiveComponent> SurfaceComponent = NULL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FVector SurfacePoint = FVector(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FVector_NetQuantizeNormal SurfaceNormal = FVector(0);

	// Get surface friction (X), surface bounciness (Y)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FVector SurfacePhysicProperties = FVector(1, 0, 0);
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

	// The current action phase
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Infos")
	TEnumAsByte<EActionPhase> CurrentPhase = EActionPhase::ActionPhase_Undetermined;


	void Init(FVector timings, float coolDown, int repeatCount = 0);

	double GetRemainingActivationTime() const;

	double GetRemainingCoolDownTime() const;

	double GetNormalizedTime(EActionPhase phase) const;

	void SkipTimeToPhase(EActionPhase phase);

	void Update(float deltaTime);

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
	FVector_NetQuantize10 StateModifiers1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	FVector_NetQuantize10 StateModifiers2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	FVector_NetQuantize10 ActionsModifiers1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusParameters")
	FVector_NetQuantize10 ActionsModifiers2;
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
	UPROPERTY(SkipSerialization)
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

	//Set the referential movement (usually the surface the controller is on)
	void SetReferentialMovement(const FVector movement, const float delta, const float acceleration = -1);

	//Add a composite movement. useful to match a certain speed;
	void AddCompositeMovement(const FVector movement, const float acceleration = -1, int index = -1);

	//Remove a composite movement at index.
	bool RemoveCompositeMovement(int index);

	//Compute an acceleration from this condition leading to the desired velocity
	FVector GetAccelerationFromVelocity(FVector desiredVelocity, double deltaTime, bool onlyContribution = false) const;

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
	FAngularKinematicCondition GetFinalCondition(double deltaTime) const;
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
	

	FKinematicComponents();

	FKinematicComponents(FLinearKinematicCondition linearCond, FAngularKinematicCondition angularCond);

	FKinematicComponents FromComponent(FKinematicComponents fromComponent, double withDelta);

	FKinematicComponents FromComponent(FKinematicComponents fromComponent, FVector linearAcceleration, double withDelta);

	//Get the rotation from angular kinematic.
	FQuat GetRotation() const;
};


//The Status of the controller including Kinematic and Status Parameters
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FControllerStatus
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcessResult")
	int DiffManifest = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcessResult")
	int SurfaceIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FKinematicComponents Kinematics = FKinematicComponents();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FStatusParameters StatusParams = FStatusParameters();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FVector MoveInput = FVector(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	TArray<FSurfaceTrackData> SurfacesInContact;




	//X= surface friction, Y=Drag, Z= Bounciness
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FVector CustomPhysicProperties = FVector(-1);

	//The current surface the controller is on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FSurfaceInfos ControllerSurface = FSurfaceInfos();




	void ComputeDiffManifest(FControllerStatus diffDatas);

	void FromStatusDiff(int diffManifest, FControllerStatus diffDatas);

	//Get the difference controller staus from Diff Manifest
	FControllerStatus GetDiffControllerStatus() const;
};


// The result of a check on state or action
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FControllerCheckResult
{
	GENERATED_BODY()

public:

	FORCEINLINE FControllerCheckResult() {}

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


//*
//* Represent an override root motion command
//*
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FOverrideRootMotionCommand
{
	GENERATED_BODY()

public:
	FORCEINLINE FOverrideRootMotionCommand()
	{
	}

	FORCEINLINE FOverrideRootMotionCommand(ERootMotionType translationMode, ERootMotionType rotationMode, float duration)
	{
		OverrideTranslationRootMotionMode = translationMode;
		OverrideRotationRootMotionMode = rotationMode;
		OverrideRootMotionChrono = duration;
	}

	//The override translation rootMotion mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Param")
	TEnumAsByte<ERootMotionType> OverrideTranslationRootMotionMode = ERootMotionType::RootMotionType_No_RootMotion;

	//The override rotation rootMotion mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Param")
	TEnumAsByte<ERootMotionType> OverrideRotationRootMotionMode = ERootMotionType::RootMotionType_No_RootMotion;

	//The chrono to switch back override root motion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Param")
	float OverrideRootMotionChrono = 0;
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
