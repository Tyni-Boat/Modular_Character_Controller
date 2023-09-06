// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Enums.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/MovementComponent.h"
#include "Structs.generated.h"


#pragma region Misc


USTRUCT(BlueprintType)
struct FMathExtension
{
	GENERATED_BODY()

public:

	/// <summary>
	/// Convert from a bool array to an integer. useful to serialize indexes in an array.
	/// </summary>
	FORCEINLINE static int BoolArrayToInt(const TArray<bool> array)
	{
		//Translate to int
		TArray<int> arr;
		for (int i = 0; i < array.Num(); i++)
			arr.Add(array[i] ? 1 : 0);

		int result = 0;
		for (int i = 0; i < arr.Num(); i++)
		{
			result += arr[i] << (arr.Num() - i - 1);
		}
		return result;
	}

	/// <summary>
	/// Convert from an in to an bool array. useful to deserialize indexes in an array.
	/// </summary>
	FORCEINLINE static TArray<bool> IntToBoolArray(int integer)
	{
		TArray<int> binary_array;
		TArray<bool> bool_array;
		int n = integer;

		while (n > 0)
		{
			binary_array.Add(n % 2);
			n /= 2;
		}
		for (int i = 0; i < binary_array.Num(); i++)
			bool_array.Add(binary_array[binary_array.Num() - (i + 1)] >= 1);

		return bool_array;
	}

	/// <summary>
	/// Convert a bool array to an index array
	/// </summary>
	FORCEINLINE static TArray<int> BoolToIndexesArray(const TArray<bool> array)
	{
		TArray<int> indexArray;
		//loop the array and add to the int[] i if array[i] == true
		for (int i = 0; i < array.Num(); i++)
			if (array[i])
				indexArray.Add(i);
		//return int[]
		return indexArray;
	}

	/// <summary>
	/// Convert an int array of indexes to an bool array
	/// </summary>
	FORCEINLINE static TArray<bool> IndexesToBoolArray(const TArray<int> array)
	{
		//Get the highest index
		int length = array.Max();
		//set the bool[] size
		TArray<bool> bools;
		for (int i = 0; i < length; i++)
			bools.Add(false);
		//loop the array and true at bool[x], where x = arr[i]
		for (int i = 0; i < array.Num(); i++)
		{
			if (!bools.IsValidIndex(array[i]))
				continue;
			bools[array[i]] = true;
		}

		//return bool[]
		return bools;
	}

};


#pragma endregion


#pragma region Inputs


/*
* Input entry structure. InputEntryNature_Axis X should be used for value types.
*/
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FInputEntry
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputEntry() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
		TEnumAsByte<EInputEntryNature> Nature = EInputEntryNature::InputEntryNature_Button;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
		TEnumAsByte<EInputEntryType> Type = EInputEntryType::InputEntryType_Simple;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
		FVector_NetQuantize Axis;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
		float InputBuffer = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
		TEnumAsByte<EInputEntryPhase> Phase = EInputEntryPhase::InputEntryPhase_None;


	float _bufferChrono = 0;
	float _activeDuration = 0;

	///// <summary>
	///// Check if this input is Still valid, updating it's values
	///// </summary>
	FORCEINLINE bool IsObsolete(float d)
	{
		bool result = Phase == EInputEntryPhase::InputEntryPhase_Released;
		_activeDuration += d;
		switch (Nature)
		{
		case EInputEntryNature::InputEntryNature_Axis: {
			Axis = FVector(0);
			result = false;
		}break;
		case EInputEntryNature::InputEntryNature_Value: {
			result = false;
		}break;
		default:
		{
			switch (Type)
			{
			case EInputEntryType::InputEntryType_Buffered: {
				if (Phase == EInputEntryPhase::InputEntryPhase_Released)
				{
					_bufferChrono += d;
					if (_bufferChrono <= InputBuffer)
					{
						result = false;
					}
				}
				else
				{
					_bufferChrono = 0;
				}
			}break;
			}
		}break;
		}
		return result;
	}

};


/*
* Input entry structure for network transmissions
*/
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FNetInputPair
{
	GENERATED_BODY()

public:
	FORCEINLINE FNetInputPair() {}

	FORCEINLINE FNetInputPair(FName name, FInputEntry entry)
	{
		Key = name;
		Value = entry;
	}

	UPROPERTY()
		FName Key;
	UPROPERTY()
		FInputEntry Value;
};


/*
* Represent a pack of input entry, tracking inputs. Used locally only. not intended to be used remotely
*/
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FInputEntryPool
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputEntryPool() {}

	FORCEINLINE FInputEntryPool(const FInputEntryPool& ref)
	{
		_inputPool.Empty();
		_inputPool_last.Empty();

		for (auto entry : ref._inputPool)
			_inputPool.Add(entry.Key, entry.Value);
		for (auto entry : ref._inputPool_last)
			_inputPool_last.Add(entry.Key, entry.Value);
	}

	//The input pool
	TMap<FName, FInputEntry> _inputPool;

	//The input pool of the last frame
	TMap<FName, FInputEntry> _inputPool_last;



	/// <summary>
	/// Add input to the input pool. return true when added not replaced
	/// </summary>
	FORCEINLINE bool AddOrReplace(FName key, FInputEntry entry)
	{
		if (key.IsNone())
			return false;

		if (_inputPool_last.Contains(key) && _inputPool_last[key].Type == entry.Type)
		{
			entry.Phase = EInputEntryPhase::InputEntryPhase_Held;
			entry._activeDuration = _inputPool_last[key]._activeDuration;
			entry._bufferChrono = _inputPool_last[key]._bufferChrono;
			_inputPool_last[key] = entry;
			if (_inputPool.Contains(key))
				_inputPool[key] = entry;
			else
				_inputPool.Add(key, entry);
			return false;
		}
		else if (_inputPool_last.Contains(key) && _inputPool_last[key].Type != entry.Type)
		{
			entry.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
			entry._activeDuration = _inputPool_last[key]._activeDuration;
			entry._bufferChrono = 0;
			_inputPool_last[key] = entry;
			if (_inputPool.Contains(key))
				_inputPool[key] = entry;
			else
				_inputPool.Add(key, entry);
		}
		else if (_inputPool.Contains(key))
		{
			entry.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
			_inputPool[key] = entry;
		}
		else
		{
			entry.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
			_inputPool.Add(key, entry);
		}
		return true;
	}

	/// <summary>
	/// Get input from the inputs pool
	/// </summary>
	FORCEINLINE FInputEntry ReadInput(FName key) const
	{
		FInputEntry entry = FInputEntry();
		if (_inputPool_last.Contains(key))
		{
			entry.Nature = _inputPool_last[key].Nature;
			entry.Type = _inputPool_last[key].Type;
			entry.Phase = _inputPool_last[key].Phase;
			entry.Axis = _inputPool_last[key].Axis;
			if (entry.Type == EInputEntryType::InputEntryType_Buffered && entry.Phase == EInputEntryPhase::InputEntryPhase_Released)
			{
				entry.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
			}
		}
		else
		{
			entry.Nature = EInputEntryNature::InputEntryNature_Button;
			entry.Type = EInputEntryType::InputEntryType_Simple;
			entry.Phase = EInputEntryPhase::InputEntryPhase_None;
			entry.Axis = FVector(0);
		}
		return entry;
	}

	/// <summary>
	/// Read an input and consume it.
	/// </summary>
	/// <param name="key"></param>
	/// <returns></returns>
	FORCEINLINE FInputEntry ConsumeInput(FName key)
	{
		FInputEntry entry = FInputEntry();
		entry = ReadInput(key);
		if(_inputPool_last.Contains(key))
			_inputPool_last.Remove(key);
		return entry;
	}

	/// <summary>
	/// Update the inputs pool
	/// </summary>
	FORCEINLINE void UpdateInputs(float delta)
	{
		//New commers
		for (auto& entry : _inputPool)
		{
			if (!_inputPool_last.Contains(entry.Key))
			{
				auto input = entry.Value;
				input.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
				input._activeDuration = 0;
				input._bufferChrono = 0;
				_inputPool_last.Add(entry.Key, input);
			}
			else
			{
				_inputPool_last[entry.Key].Phase = EInputEntryPhase::InputEntryPhase_Held;
				_inputPool_last[entry.Key]._activeDuration += delta;
				_inputPool_last[entry.Key].Axis = entry.Value.Axis;
			}
		}

		//Gones
		TArray<FName> toRemove;
		for (auto& entry : _inputPool_last)
		{
			if (!_inputPool.Contains(entry.Key))
			{
				_inputPool_last[entry.Key].Phase = EInputEntryPhase::InputEntryPhase_Released;
				if (_inputPool_last[entry.Key].IsObsolete(delta))
					toRemove.Add(entry.Key);
			}
		}

		//Remove garbage
		for (auto& key : toRemove)
		{
			if (_inputPool_last.Contains(key))
			{
				_inputPool_last.Remove(key);
			}
		}

		_inputPool.Empty();
	}
};


/// <summary>
/// Special input type only for network transmission.
/// </summary>
USTRUCT(BlueprintType)
struct FNetInputsPool
{
	GENERATED_BODY()

public:

	//The input pool (network)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "network")
		TArray<FNetInputPair> _inputPool_net;

	FORCEINLINE void FromInputs(const FInputEntryPool& referenceInputs)
	{
		_inputPool_net.Empty();
		for (auto pair : referenceInputs._inputPool_last)
		{
			FInputEntry entry = pair.Value;
			entry.Axis *= 100;
			_inputPool_net.Add(FNetInputPair(pair.Key, entry));
		}
	}

	FORCEINLINE void ToInputs(FInputEntryPool& referenceInputs)
	{
		for (auto pair : _inputPool_net)
		{
			FInputEntry entry = pair.Value;
			entry.Axis /= 100;
			referenceInputs.AddOrReplace(pair.Key, entry);
		}
	}
};



#pragma endregion


#pragma region Surface and Zones


/// <summary>
/// State Behavior ability to track surface velocity. Intended to be used in local
/// </summary>
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FSurfaceInfos
{
	GENERATED_BODY()

public:

	FORCEINLINE FSurfaceInfos()
	{
	}

	/**
	 * @brief Update surface infos velocities
	 * @param controller
	 * @param selectedSurface
	 */
	FORCEINLINE void UpdateSurfaceInfos(AActor* actor, const FHitResult selectedSurface, const float delta)
	{
		if (actor == nullptr)
			return;

		_surfaceHitResult = selectedSurface;
		_surfaceNormal = selectedSurface.Normal;

		//We're on the same surface
		if (_currentSurface.Get() != nullptr && selectedSurface.GetComponent() != nullptr && _currentSurface.Get() == selectedSurface.GetComponent())
		{
			FTransform surfaceTransform = _currentSurface->GetComponentTransform();
			FVector look = surfaceTransform.TransformVector(_surfaceLocalLookDir);
			FVector pos = surfaceTransform.TransformPosition(_surfaceLocalHitPoint);

			//Velocity
			{
				//Linear Part
				FVector bodyVelocity = FVector(0);
				UActorComponent* movementComponent = nullptr;
				if (selectedSurface.GetActor())
				{
					movementComponent = selectedSurface.GetActor()->GetComponentByClass(UMovementComponent::StaticClass());
				}
				if (selectedSurface.GetComponent()->IsSimulatingPhysics())
				{
					bodyVelocity = selectedSurface.GetComponent()->GetPhysicsLinearVelocity(selectedSurface.BoneName) * delta;
				}
				else if (movementComponent)
				{
					bodyVelocity = Cast<UMovementComponent>(movementComponent)->Velocity * delta;
				}
				else
				{
					bodyVelocity = selectedSurface.GetComponent()->GetComponentLocation() - _currentSurface_Location;
				}

				//Angular part
				FQuat currentPl_quat = selectedSurface.GetComponent()->GetComponentRotation().Quaternion();
				FQuat lastPl_quat = _currentSurface_Rotation;
				lastPl_quat.EnforceShortestArcWith(currentPl_quat);
				FQuat pl_rotDiff = currentPl_quat * lastPl_quat.Inverse();
				float angle;
				FVector axis;
				pl_rotDiff.ToAxisAndAngle(axis, angle);
				FVector dir, up, fwd;
				up = axis;
				fwd = (actor->GetActorLocation() - _currentSurface->GetComponentLocation()).GetSafeNormal();
				dir = FVector::CrossProduct(up, fwd);
				float r = (actor->GetActorLocation() - _currentSurface->GetComponentLocation()).Length();
				FVector rotVel = r * angle * dir;

				//Finally
				//_surfaceVelocity = (pos - controller->GetLocation());
				_surfaceLinearCompositeVelocity = bodyVelocity;
				_surfaceAngularCompositeVelocity = rotVel;
			}

			//Orientation
			{
				FQuat targetQuat = selectedSurface.GetComponent()->GetComponentRotation().Quaternion();
				FQuat currentQuat = _currentSurface_Rotation;

				//Get Angular speed
				currentQuat.EnforceShortestArcWith(targetQuat);
				auto quatDiff = targetQuat * currentQuat.Inverse();
				_surfaceAngularVelocity = quatDiff;
			}
		}
		//we changed surfaces
		if (_currentSurface != selectedSurface.GetComponent())
		{
			_surfaceLinearCompositeVelocity = FVector(0);
			_surfaceAngularCompositeVelocity = FVector(0);
			_surfaceAngularVelocity = FQuat::Identity;
		}
		_currentSurface = selectedSurface.GetComponent();
		if (_currentSurface != nullptr)
		{
			auto surfaceTransform = _currentSurface->GetComponentTransform();
			_surfaceLocalLookDir = surfaceTransform.InverseTransformVector(actor->GetActorRotation().Vector());
			_surfaceLocalHitPoint = surfaceTransform.InverseTransformPosition(actor->GetActorLocation());
			_currentSurface_Location = _currentSurface->GetComponentLocation();
			_currentSurface_Rotation = _currentSurface->GetComponentRotation().Quaternion();
		}
	}

	/// <summary>
	/// Reset the surface infos
	/// </summary>
	FORCEINLINE void Reset()
	{
		_currentSurface = nullptr;
		_surfaceLinearCompositeVelocity = FVector(0);
		_surfaceAngularCompositeVelocity = FVector(0);
		_surfaceAngularVelocity = FQuat::Identity;
		_surfaceLocalHitPoint = FVector(0);
		_currentSurface_Location = FVector(INFINITY);
		_currentSurface_Rotation = FQuat::Identity;
		_surfaceLocalLookDir = FVector(0);
	}

	/// <summary>
	/// Get the last evaluated linear velocity
	/// </summary>
	FORCEINLINE FVector GetSurfaceLinearVelocity(bool linear = true, bool angular = true) const
	{
		FVector velocity = FVector(0);
		if (linear)
			velocity += _surfaceLinearCompositeVelocity;
		if (angular)
			velocity += _surfaceAngularCompositeVelocity;
		return velocity;
	}

	/// <summary>
	/// Get the last evaluated angular velocity
	/// </summary>
	FORCEINLINE FQuat GetSurfaceAngularVelocity() const { return  _surfaceAngularVelocity; }

	/// <summary>
	/// Get the last evaluated surface normal
	/// </summary>
	FORCEINLINE FVector GetSurfaceNormal() const { return  _surfaceNormal; }

	/// <summary>
	/// Get surface primitive
	/// </summary>
	FORCEINLINE UPrimitiveComponent* GetSurfacePrimitive() const { return  _currentSurface.Get(); }

	/// <summary>
	/// Get surface hit result data
	/// </summary>
	FORCEINLINE FHitResult GetHitResult() const { return  _surfaceHitResult; }

public:

	//the surface hit raycast
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
		FHitResult _surfaceHitResult;

protected:
	//------------------------------------------------------------------------------------------

	//the current surface
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
		TSoftObjectPtr<UPrimitiveComponent> _currentSurface;

	//the surface linear velocity
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
		FVector _surfaceLinearCompositeVelocity = FVector(0);

	//the surface angular velocity
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
		FVector _surfaceAngularCompositeVelocity = FVector(0);

	//the surface normal
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
		FVector _surfaceNormal = FVector(0);

	//the surface angular velocity
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
		FQuat _surfaceAngularVelocity = FQuat(0);

	//the surface velocity
	FVector _surfaceLocalHitPoint;

	//the surface velocity
	FVector _surfaceLocalLookDir;

	//the absolute location of the platform during last frame
	FVector _currentSurface_Location = FVector(INFINITY);

	//The absolute rotation of the platform during last frame
	FQuat _currentSurface_Rotation;
};



/// <summary>
/// Surface representation on network transmissions
/// </summary>
/// <returns></returns>
USTRUCT(BlueprintType)
struct FNetSurfaceInfos
{
	GENERATED_BODY()

public:

	//the surface hit raycast
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Network Values")
		FHitResult SurfaceHitResult;

	/// <summary>
	/// Copy params from a surface
	/// </summary>
	FORCEINLINE void FromSurface(const FSurfaceInfos& surface)
	{
		SurfaceHitResult = surface.GetHitResult();
	}

	/// <summary>
	/// Restitute surface from this.
	/// </summary>
	FORCEINLINE void ToSurface(FSurfaceInfos& surface)
	{
		surface._surfaceHitResult = SurfaceHitResult;
	}
};



#pragma endregion



#pragma region MovementInfosAndReplication

//*
//* InitialVelocities informations
//*
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FVelocity
{
	GENERATED_BODY()

public:
	FORCEINLINE FVelocity()
	{
		ConstantLinearVelocity = FVector(0);
		InstantLinearVelocity = FVector(0);
		Rotation = FQuat(0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Velocity")
		FVector_NetQuantize ConstantLinearVelocity = FVector(0);

	UPROPERTY(SkipSerialization, EditAnywhere, BlueprintReadWrite, Category = "Velocity")
		FVector_NetQuantize InstantLinearVelocity = FVector(0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Velocity")
		FQuat Rotation = FQuat(0);

	//to scale the root motion or actiate/desactivate it
	UPROPERTY(SkipSerialization, EditAnywhere, BlueprintReadWrite, Category = "Velocity")
		float _rooMotionScale = 1;



	static FVelocity Null()
	{
		FVelocity result = FVelocity();
		result.Rotation = FQuat(0);
		result.ConstantLinearVelocity = FVector(0);
		result.InstantLinearVelocity = FVector(0);
		return result;
	}

	static FVector AngularVelocityFromRotation(FQuat rotation)
	{
		FVector axis;
		float angle;
		rotation.ToAxisAndAngle(axis, angle);
		float thisAngle = angle;
		float maxAngle = FMath::DegreesToRadians(360);
		auto res = FMath::Floor(thisAngle / maxAngle);
		res = res * maxAngle;
		angle = thisAngle - res;
		return axis * angle;
	}

	static FQuat RotationDeltaFromAngularVelocity(FVector angularVelocity)
	{
		FVector axis = angularVelocity.GetSafeNormal();
		float thisAngle = angularVelocity.Length();
		float maxAngle = FMath::DegreesToRadians(360);
		auto res = FMath::Floor(thisAngle / maxAngle);
		res = res * maxAngle;
		float angle = thisAngle - res;
		return FQuat(axis, angle);
	}
};



//* Move Simulation infos
//*/
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FKinematicInfos
{
	GENERATED_BODY()

public:
	FORCEINLINE FKinematicInfos()
	{
	}

	FORCEINLINE FKinematicInfos(UMovementComponent* byComponent, const  FVector InGravity, const  FKinematicInfos fromLastMove, const float inMass = 0, const bool inDebug = false)
	{
		if (byComponent)
			actor = byComponent->GetOwner();
		Gravity = InGravity;
		InitialTransform = fromLastMove.FinalTransform;
		InitialVelocities = fromLastMove.FinalVelocities;
		InitialSurface = fromLastMove.FinalSurface;
		InitialActionsIndexes = fromLastMove.FinalActionsIndexes;
		InitialStateIndex = fromLastMove.FinalStateIndex;
		Mass = inMass;
		IsDebugMode = inDebug;
	}

	FORCEINLINE FKinematicInfos(const FTransform fromTransform, const  FVelocity fromVelocity, const  FSurfaceInfos onSurface, const int inStateIndex, const TArray<int> inActionIndexes)
	{
		InitialTransform = fromTransform;
		InitialVelocities = fromVelocity;
		InitialSurface = onSurface;
		InitialStateIndex = inStateIndex;
		InitialActionsIndexes.Empty();
		for (int i = 0; i < inActionIndexes.Num(); i++)
			InitialActionsIndexes.Add(inActionIndexes[i]);
	}

	FORCEINLINE void ChangeActor(const UMovementComponent* byComponent, const FName primitiveSocket = "", const bool inDebug = false)
	{
		if (byComponent == nullptr)
			return;

		actor = byComponent->GetOwner();
		IsDebugMode = inDebug;
		UPrimitiveComponent* primitive = byComponent->UpdatedPrimitive;
		if (primitive == nullptr)
			return;
		_primitive_shape = primitive->GetCollisionShape();
	}

	FORCEINLINE void FromInitialValues(const FKinematicInfos& ref, bool copyFinals = false)
	{
		InitialSurface = ref.InitialSurface;
		InitialStateIndex = ref.InitialStateIndex;
		InitialActionsIndexes = ref.InitialActionsIndexes;
		InitialVelocities = ref.InitialVelocities;
		InitialTransform = ref.InitialTransform;
		if (copyFinals)
		{
			FinalSurface = ref.FinalSurface;
			FinalStateIndex = ref.FinalStateIndex;
			FinalActionsIndexes = ref.FinalActionsIndexes;
			FinalVelocities = ref.FinalVelocities;
			FinalTransform = ref.FinalTransform;

		}
		else
		{
			FinalSurface = FSurfaceInfos();
			FinalStateIndex = -1;
			FinalActionsIndexes.Empty();
			FinalVelocities = FVelocity();
			FinalTransform = FTransform::Identity;
		}
	}


	//Velocities *************************************************************************************

	/// <summary>
	/// The velocities component, containing both velocities and accelerations at the initial position
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Velocities")
		FVelocity InitialVelocities;

	/// <summary>
	/// The velocities component, containing both velocities and accelerations at the final state
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Velocities")
		FVelocity FinalVelocities;


	//Positionning *************************************************************************************

	/// <summary>
	/// The final position, rotation, scale after movement
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Positionning")
		FTransform FinalTransform = FTransform::Identity;

	/// <summary>
	/// The initial position, rotation, scale before movement
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Positionning")
		FTransform InitialTransform = FTransform::Identity;


	//Surfaces *************************************************************************************

	/// <summary>
	/// The final surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Surfaces")
		FSurfaceInfos FinalSurface;

	/// <summary>
	/// The initial surface
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Surfaces")
		FSurfaceInfos InitialSurface;


	//State and Actions *************************************************************************************

	/// <summary>
	/// The initial state index
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "State and Actions")
		int InitialStateIndex = -1;

	/// <summary>
	/// The final state index
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "State and Actions")
		int FinalStateIndex = -1;

	/// <summary>
	/// The current state's flag, often used as binary. to relay this behaviour's state over the network. Can be used for things like behaviour phase.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Behaviours")
		int FinalStateFlag = 0;

	/// <summary>
	/// The initial actions indexes
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "State and Actions")
		TArray<int> InitialActionsIndexes;

	/// <summary>
	/// The final actions indexes
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "State and Actions")
		TArray<int> FinalActionsIndexes;


	//Physic *************************************************************************************

	//Do we use physic interractions?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Physics")
		bool bUsePhysic;

	//Others *************************************************************************************


	/// <summary>
	/// The actor's Mass
	/// </summary>
	double Mass;

	/// <summary>
	/// The Gravity
	/// </summary>
	FVector_NetQuantize Gravity = -FVector_NetQuantize::UpVector;




	//The actor making the movement
	TSoftObjectPtr<AActor> actor;

	//Debug mode in enabled?
	bool IsDebugMode;

	//The name of the current state
	FName _currentStateName;

	//The shape of the updated primitive
	FCollisionShape _primitive_shape;


	/// <summary>
	/// Get the kinematic action making the movement.
	/// </summary>
	/// <returns></returns>
	FORCEINLINE AActor* GetActor() const { return  actor.Get(); }

	/// <summary>
	/// Get the actor's Mass
	/// </summary>
	/// <returns></returns>
	FORCEINLINE float GetMass() const { return  Mass; }

	/// <summary>
	/// Get initial ascension scale; positive value if the momentum goes against the gravity
	/// </summary>
	FORCEINLINE float GetInitialAscensionScale() const
	{
		const float dotProduct = FVector::DotProduct(GetInitialMomentum(), -Gravity.GetSafeNormal());
		return dotProduct;
	}

	/// <summary>
	/// Get the initial momentum.
	/// </summary>
	/// <returns></returns>
	FORCEINLINE FVector GetInitialMomentum() const { return InitialVelocities.ConstantLinearVelocity; }// + InitialVelocities.InstantLinearVelocity; }

	/// <summary>
	/// Get the final momentum.
	/// </summary>
	/// <returns></returns>
	FORCEINLINE FVector GetFinalMomentum() const { return InitialVelocities.ConstantLinearVelocity; }// + InitialVelocities.InstantLinearVelocity;}

	/// <summary>
	/// Predict velocities at time T [v = v0 + at]. Assuming initial and final velocities are corrects for the delta time
	/// </summary>
	/// <returns></returns>
	FORCEINLINE FVelocity PredictVelocity(float time) const
	{
		FVelocity result = FVelocity::Null();
		result.ConstantLinearVelocity = InitialVelocities.ConstantLinearVelocity + (FinalVelocities.ConstantLinearVelocity - InitialVelocities.ConstantLinearVelocity) * time;
		return result;
	}

	/// <summary>
	/// Predict Transform at Time. Assuming initial and final velocities are corrects for the delta time
	/// </summary>
	/// <returns></returns>
	FORCEINLINE FTransform PredictTransform(float time) const
	{
		FTransform result = InitialTransform;

		//Predict location (x = x0 + v0t + 1/2at2)
		{
			FVector acceleration = (FinalVelocities.ConstantLinearVelocity - InitialVelocities.ConstantLinearVelocity);
			FVector location = FinalTransform.GetLocation() + FinalVelocities.ConstantLinearVelocity * time + (0.5f * acceleration * FMath::Pow(time, 2));
			result.SetLocation(location);
		}

		//Predict Rotation (2adR = w2 + wo2)
		{
			FQuat rotation = FinalTransform.GetRotation();
			result.SetRotation(rotation);
		}

		return result;
	}
};


/// <summary>
/// Designed to transmit movement over the network
/// </summary>
USTRUCT(BlueprintType)
struct FNetKinematicMoveInfos
{
	GENERATED_BODY()

public:

	//Velocities *************************************************************************************

	/// <summary>
	/// The velocities component, containing both velocities and accelerations at the initial position
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Velocities")
		FVelocity Velocities;


	//Transform *************************************************************************************

	/// <summary>
	/// The position.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Positionning")
		FVector_NetQuantize Location;

	/// <summary>
	/// The rotation.
	/// </summary>
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Positionning")
	//	FQuat Rotation;


	//Surfaces *************************************************************************************

	/// <summary>
	/// The surface
	/// </summary>
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Surfaces")
	//	FNetSurfaceInfos Surface;


	//State and Actions *************************************************************************************

	/// <summary>
	/// The state index
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "State and Actions")
		int StateIndex = -1;

	/// <summary>
	/// The current state's flag, often used as binary. to relay this behaviour's state over the network. Can be used for things like behaviour phase.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, category = "Behaviours")
		int StateFlag = 0;

	/// <summary>
	/// The initial actions indexes. is used as binary representing indexes on an array
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "State and Actions")
		int ActionsIndexes_BinaryRepresentation;

public:

	//Copy from a kinematic move
	FORCEINLINE void FromKinematicMove(const FKinematicInfos& move)
	{
		Velocities = move.FinalVelocities;
		Location = move.InitialTransform.GetLocation();
		//Rotation = move.InitialTransform.GetRotation();
		//Surface.FromSurface(move.InitialSurface);
		StateFlag = move.FinalStateFlag;
		StateIndex = move.FinalStateIndex;
		ActionsIndexes_BinaryRepresentation = FMathExtension::BoolArrayToInt(FMathExtension::IndexesToBoolArray(move.FinalActionsIndexes));
	}

	//Restore kinematic move
	FORCEINLINE void ToKinematicMove(FKinematicInfos& move, bool rotationToAngularRot = false)
	{
		move.FinalVelocities = Velocities;
		FQuat endRotation = Velocities.Rotation;
		if (rotationToAngularRot)
		{
			move.FinalVelocities.Rotation = endRotation;
		}
		move.InitialTransform.SetComponents(endRotation, Location, FVector::One());
		//Surface.ToSurface(move.InitialSurface);
		move.FinalStateFlag = StateFlag;
		move.FinalStateIndex = StateIndex;
		move.FinalActionsIndexes = FMathExtension::BoolToIndexesArray(FMathExtension::IntToBoolArray(ActionsIndexes_BinaryRepresentation));
	}
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




//*
//* Client to server move request
//*
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FSyncMoveRequest
{
	GENERATED_BODY()

public:
	FORCEINLINE FSyncMoveRequest()
	{
	}

	FORCEINLINE FSyncMoveRequest(FKinematicInfos& move, FInputEntryPool& inputsMade, long timeStamp, float deltaTime)
	{
		MoveInfos.FromKinematicMove(move);
		Inputs.FromInputs(inputsMade);
		TimeStamp = timeStamp;
		DeltaTime = deltaTime;
	}

	UPROPERTY()
		FNetKinematicMoveInfos MoveInfos;
	UPROPERTY()
		FNetInputsPool Inputs;
	UPROPERTY()
		int TimeStamp = 0;
	UPROPERTY()
		double DeltaTime = 0;
};


#pragma endregion



#pragma region Actions


//*
//* Represent an action montage parameter
//*
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FActionMotionMontage
{
	GENERATED_BODY()

public:
	FORCEINLINE FActionMotionMontage()
	{
	}

	/// <summary>
	/// The Animation Montages to play
	/// </summary>
	UPROPERTY(EditAnywhere, Category = "Action|Types|Montage")
		TSoftObjectPtr<UAnimMontage> Montage;

	/// <summary>
	/// The Animation Montages section to play
	/// </summary>
	UPROPERTY(EditAnywhere, Category = "Action|Types|Montage")
		FName MontageSection;
};


#pragma endregion


#pragma region Extensions

//Extension methods for structures
UCLASS(BlueprintType)
class UStructExtensions : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "FInputEntry")
		static bool IsObsolete(FInputEntry input, FInputEntry& output, float d)
	{
		bool ret = input.IsObsolete(d);
		output = input;
		return ret;
	}

	UFUNCTION(BlueprintCallable, Category = "FInputEntryPool")
		static bool ListenInput(FInputEntryPool input, FInputEntryPool& output, FName key, FInputEntry entry)
	{
		bool ret = input.AddOrReplace(key, entry);
		output = input;
		return ret;
	}

	UFUNCTION(BlueprintCallable, Category = "FInputEntryPool")
		static FInputEntry ReadInput(const FInputEntryPool MyStructRef, FName key)
	{
		return MyStructRef.ReadInput(key);
	}

	UFUNCTION(BlueprintCallable, Category = "FInputEntryPool")
		static FInputEntry ConsumeInput(FInputEntryPool MyStructRef, FName key)
	{
		return MyStructRef.ConsumeInput(key);
	}

	UFUNCTION(BlueprintCallable, Category = "FSurfaceInfos")
		static FVector GetSurfaceLinearVelocity(const FSurfaceInfos MyStructRef, bool linear = true, bool angular = true)
	{
		return MyStructRef.GetSurfaceLinearVelocity(linear, angular);
	}

	UFUNCTION(BlueprintCallable, Category = "FSurfaceInfos")
		static FQuat GetSurfaceAngularVelocity(const FSurfaceInfos MyStructRef)
	{
		return MyStructRef.GetSurfaceAngularVelocity();
	}

	UFUNCTION(BlueprintCallable, Category = "FSurfaceInfos")
		static FHitResult GetSurfaceHitInfos(const FSurfaceInfos MyStructRef)
	{
		return MyStructRef.GetHitResult();
	}

	UFUNCTION(BlueprintCallable, Category = "FVelocity")
		static FVector AngularVelocityFromRotation(const FVelocity MyStructRef, FQuat rot)
	{
		return FVelocity::AngularVelocityFromRotation(rot);
	}

	UFUNCTION(BlueprintCallable, Category = "FVelocity")
		static FQuat RotationDeltaFromAngularVelocity(const FVelocity MyStructRef, FVector angularRot)
	{
		return FVelocity::RotationDeltaFromAngularVelocity(angularRot);
	}

	UFUNCTION(BlueprintCallable, Category = "FKinematicInfos")
		static AActor* GetActor(const FKinematicInfos MyStructRef)
	{
		return MyStructRef.GetActor();
	}

	UFUNCTION(BlueprintCallable, Category = "FKinematicInfos")
		static float GetMass(const FKinematicInfos MyStructRef)
	{
		return MyStructRef.GetMass();
	}

	UFUNCTION(BlueprintCallable, Category = "FKinematicInfos")
		static float GetInitialAscensionScale(const FKinematicInfos& MyStructRef)
	{
		return MyStructRef.GetInitialAscensionScale();
	}

	UFUNCTION(BlueprintCallable, Category = "FKinematicInfos")
		static FVector GetInitialMomentum(const FKinematicInfos MyStructRef)
	{
		return MyStructRef.GetInitialMomentum();
	}

	UFUNCTION(BlueprintCallable, Category = "FKinematicInfos")
		static FVector GetFinalMomentum(const FKinematicInfos MyStructRef)
	{
		return MyStructRef.GetFinalMomentum();
	}

	UFUNCTION(BlueprintCallable, Category = "FKinematicInfos")
		static FVector GetGravity(const FKinematicInfos MyStructRef)
	{
		return MyStructRef.Gravity;
	}

	template <typename T>
	UFUNCTION(BlueprintCallable, Category = "Common Objects")
	static std::enable_if_t<std::is_base_of_v<UObject, T>, T> GetObject(const TSoftObjectPtr<T> softObj)
	{
		if (!softObj.IsValid())
			return nullptr;
		return softObj.Get();
	}

};

#pragma endregion