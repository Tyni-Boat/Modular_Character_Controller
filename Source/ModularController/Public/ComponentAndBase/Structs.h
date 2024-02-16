// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Enums.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/MovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
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
			result += arr[i] * TwoPowX(i);
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
		for (int i = binary_array.Num() - 1; i >= 0; i--)
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

	/// <summary>
	/// Returns a power of ten.
	/// </summary>
	/// <param name="exponent"></param>
	/// <returns></returns>
	FORCEINLINE static double TenPowX(const unsigned int exponent)
	{
		if (exponent == 0)
			return 1;
		if (exponent == 1)
			return 10;
		double result = 10;
		for (unsigned int i = 1; i < exponent; i++)
			result *= 10;
		return result;
	}

	/// <summary>
	/// Returns a power of two.
	/// </summary>
	/// <param name="exponent"></param>
	/// <returns></returns>
	FORCEINLINE static double TwoPowX(const unsigned int exponent)
	{
		if (exponent == 0)
			return 1;
		if (exponent == 1)
			return 2;
		double result = 2;
		for (unsigned int i = 1; i < exponent; i++)
			result *= 2;
		return result;
	}


	/// <summary>
	/// Debug a boolean array
	/// </summary>
	/// <param name="array"></param>
	/// <returns></returns>
	FORCEINLINE static FString DebugBoolArray(TArray<bool> array)
	{
		FString result = FString::Printf(TEXT("{"));

		for (int i = 0; i < array.Num(); i++)
		{
			if (i > 0)
				result.Append(FString::Printf(TEXT(",")));
			result.Append(FString::Printf(TEXT("%d"), array[i]));
		}

		result.Append(FString::Printf(TEXT("}")));
		return  result;
	}


	///Match two arrays to the greatest.
	template<typename InElementType>
	FORCEINLINE
		static void MatchArraySizesToLargest(TArray<InElementType>& arrayA, TArray<InElementType>& arrayB)
	{
		if (arrayA.Num() == arrayB.Num())
			return;
		if (arrayA.Num() > arrayB.Num())
		{
			for (int i = arrayB.Num(); i < arrayA.Num(); i++)
			{
				arrayB.Add({});
			}
		}
		else
		{
			for (int i = arrayA.Num(); i < arrayB.Num(); i++)
			{
				arrayA.Add({});
			}
		}
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
	FVector Axis;
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

	FORCEINLINE bool IsActiveButton() const
	{
		if (Nature != EInputEntryNature::InputEntryNature_Button)
			return false;
		return Phase == InputEntryPhase_Pressed || Phase == InputEntryPhase_Held;
	}
};


/*
* Represent a pack of input entry, tracking inputs. Used locally only. not intended to be used remotely
*/
UCLASS(BlueprintType)
class MODULARCONTROLLER_API UInputEntryPool: public UObject
{
	GENERATED_BODY()

public:
	//FORCEINLINE UInputEntryPool() {}

	//FORCEINLINE UInputEntryPool(const UInputEntryPool& ref)
	//{
	//	_inputPool.Empty();
	//	_inputPool_last.Empty();

	//	for (auto entry : ref._inputPool)
	//		_inputPool.Add(entry.Key, entry.Value);
	//	for (auto entry : ref._inputPool_last)
	//		_inputPool_last.Add(entry.Key, entry.Value);
	//}

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

		//entry.Axis = entry.Axis.GetClampedToMaxSize(1);
		entry.Axis = entry.Axis;

		if (_inputPool.Contains(key))
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
	FORCEINLINE FInputEntry ReadInput(FName key, bool debug = false, UObject* worldContext = NULL) const
	{
		FInputEntry entry = FInputEntry();
		bool validInput = false;
		if (_inputPool_last.Contains(key))
		{
			entry.Nature = _inputPool_last[key].Nature;
			entry.Type = _inputPool_last[key].Type;
			entry.Phase = _inputPool_last[key].Phase;
			entry.Axis = _inputPool_last[key].Axis;
			entry._bufferChrono = _inputPool_last[key]._bufferChrono;
			entry._activeDuration = _inputPool_last[key]._activeDuration;
			if (entry.Type == EInputEntryType::InputEntryType_Buffered && entry.Phase == EInputEntryPhase::InputEntryPhase_Released && entry._bufferChrono > 0)
			{
				entry.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
			}
			if (entry._activeDuration > 0.2 && entry.Phase == EInputEntryPhase::InputEntryPhase_Pressed)
			{
				entry.Phase = EInputEntryPhase::InputEntryPhase_Held;
			}
			validInput = true;
		}
		else if (_inputPool.Contains(key))
		{
			entry.Nature = _inputPool[key].Nature;
			entry.Type = _inputPool[key].Type;
			entry.Phase = _inputPool[key].Phase;
			entry.Axis = _inputPool[key].Axis;
			entry._bufferChrono = _inputPool[key]._bufferChrono;
			entry._activeDuration = _inputPool[key]._activeDuration;
			if (entry.Type == EInputEntryType::InputEntryType_Buffered && entry.Phase == EInputEntryPhase::InputEntryPhase_Released)
			{
				entry.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
			}
			validInput = true;
		}
		else
		{
			entry.Nature = EInputEntryNature::InputEntryNature_Button;
			entry.Type = EInputEntryType::InputEntryType_Simple;
			entry.Phase = EInputEntryPhase::InputEntryPhase_None;
			entry.Axis = FVector(0);
		}


		if (debug && worldContext && validInput)
		{
			float bufferChrono = entry._bufferChrono;
			float activeDuration = entry._activeDuration;
			FColor debugColor;
			switch (entry.Nature)
			{
			default:
				debugColor = FColor::Silver;
				break;
			case InputEntryNature_Axis:
				debugColor = FColor::Blue;
				break;
			case InputEntryNature_Value:
				debugColor = FColor::Cyan;
				break;
			}
			UKismetSystemLibrary::PrintString(worldContext->GetWorld(), FString::Printf(TEXT("Input: (%s), Nature: (%s), Phase: (%s), buffer: %d, Held: %d"), *key.ToString(), *UEnum::GetValueAsName<EInputEntryNature>(entry.Nature).ToString(), *UEnum::GetValueAsName<EInputEntryPhase>(entry.Phase).ToString(), static_cast<int>(bufferChrono * 1000)
				, static_cast<int>(activeDuration * 1000)), true, true, debugColor, 0, key);
		}

		return entry;
	}

	/// <summary>
	/// Read an input and consume it.
	/// </summary>
	/// <param name="key"></param>
	/// <returns></returns>
	FORCEINLINE FInputEntry ConsumeInput(FName key, bool debug = false, UObject* worldContext = NULL)
	{
		FInputEntry entry = FInputEntry();
		entry = ReadInput(key, debug, worldContext);
		if (_inputPool_last.Contains(key))
			_inputPool_last.Remove(key);
		if (_inputPool.Contains(key))
			_inputPool.Remove(key);
		return entry;
	}

	/// <summary>
	/// Update the inputs pool
	/// </summary>
	FORCEINLINE void UpdateInputs(float delta)
	{
		//Update Existing
		for (auto& entry : _inputPool_last)
		{
			if (_inputPool_last[entry.Key]._bufferChrono > 0)
				_inputPool_last[entry.Key]._bufferChrono -= delta;
		}

		//New comers
		for (auto& entry : _inputPool)
		{
			if (!_inputPool_last.Contains(entry.Key))
			{
				auto input = entry.Value;
				input.Phase = EInputEntryPhase::InputEntryPhase_Pressed;
				input._activeDuration = 0;
				input._bufferChrono = input.InputBuffer;
				_inputPool_last.Add(entry.Key, input);
			}
			else
			{
				_inputPool_last[entry.Key].Phase = EInputEntryPhase::InputEntryPhase_Pressed;
				_inputPool_last[entry.Key]._activeDuration += delta;
				_inputPool_last[entry.Key].Axis = entry.Value.Axis;
				_inputPool_last[entry.Key]._bufferChrono = _inputPool_last[entry.Key].InputBuffer;
			}
		}

		//Gones
		for (auto& entry : _inputPool_last)
		{
			if (!_inputPool.Contains(entry.Key))
			{
				_inputPool_last[entry.Key].Phase = EInputEntryPhase::InputEntryPhase_Released;
				_inputPool_last[entry.Key]._activeDuration = 0;
			}
		}

		_inputPool.Empty();
	}

	FORCEINLINE void PredictInputs(UInputEntryPool from, float time, float delta)
	{
		for (auto input : from._inputPool_last)
		{
			if (!_inputPool_last.Contains(input.Key))
				continue;
			switch (input.Value.Nature)
			{
			case EInputEntryNature::InputEntryNature_Axis:
				_inputPool_last[input.Key].Axis += (_inputPool_last[input.Key].Axis - input.Value.Axis) * (time / delta);
				_inputPool_last[input.Key].Axis = _inputPool_last[input.Key].Axis.GetClampedToMaxSize(1);
				break;
			case EInputEntryNature::InputEntryNature_Value:
				_inputPool_last[input.Key].Axis.X += (_inputPool_last[input.Key].Axis.X - input.Value.Axis.X) * (time / delta);
				_inputPool_last[input.Key].Axis = _inputPool_last[input.Key].Axis.GetClampedToMaxSize(1);
				break;
			case EInputEntryNature::InputEntryNature_Button:
			{
				if (_inputPool_last[input.Key].Type == EInputEntryType::InputEntryType_Buffered)
				{
					_inputPool_last[input.Key]._bufferChrono += time;
					if (_inputPool_last[input.Key]._bufferChrono >= _inputPool_last[input.Key].InputBuffer)
					{
						if (_inputPool_last[input.Key].Phase == InputEntryPhase_Pressed)
							_inputPool_last[input.Key].Phase = InputEntryPhase_Released;
						if (_inputPool_last[input.Key].Phase == InputEntryPhase_Held)
							_inputPool_last[input.Key]._activeDuration += time;
					}
				}
				else
				{
					if (_inputPool_last[input.Key].Phase == InputEntryPhase_Pressed)
						_inputPool_last[input.Key].Phase = InputEntryPhase_Released;
					if (_inputPool_last[input.Key].Phase == InputEntryPhase_Held)
						_inputPool_last[input.Key]._activeDuration += time;
				}
			}
			break;
			}
		}
	}
};


/// <summary>
/// Encoded input for net transmission.
/// </summary>
USTRUCT(BlueprintType)
struct FTranscodedInput
{
	GENERATED_BODY()

public:

	FORCEINLINE FTranscodedInput()
	{
		axisCode = -1;
		valuesCode = -1;
		buttonsCode = -1;
	}

	UPROPERTY()
	double axisCode = -1;

	UPROPERTY()
	double valuesCode = -1;

	UPROPERTY()
	int buttonsCode = -1;
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
	FORCEINLINE void UpdateSurfaceInfos(FTransform inTransform, const FHitResult selectedSurface, const float delta)
	{
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
				fwd = (inTransform.GetLocation() - _currentSurface->GetComponentLocation()).GetSafeNormal();
				dir = FVector::CrossProduct(up, fwd);
				float r = (inTransform.GetLocation() - _currentSurface->GetComponentLocation()).Length();
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
			_surfaceLocalLookDir = surfaceTransform.InverseTransformVector(inTransform.GetRotation().Vector());
			_surfaceLocalHitPoint = surfaceTransform.InverseTransformPosition(inTransform.GetLocation());
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


#pragma endregion



#pragma region States and Actions


//*
//* Represent an action montage parameter
//*
USTRUCT(BlueprintType)
struct MODULARCONTROLLER_API FActionMotionMontage
{
	GENERATED_BODY()

public:
	FORCEINLINE FActionMotionMontage() {}

	/// <summary>
	/// The Animation Montages to play
	/// </summary>
	UPROPERTY(EditAnywhere, Category = "Action|Types|Montage")
	//TSoftObjectPtr<UAnimMontage> Montage;
	UAnimMontage* Montage;

	/// <summary>
	/// The Animation Montages section to play
	/// </summary>
	UPROPERTY(EditAnywhere, Category = "Action|Types|Montage")
	FName MontageSection;
};


/// <summary>
/// The infos abput the state and actions of the controller
/// </summary>
USTRUCT(BlueprintType)
struct FStatusParameters
{
	GENERATED_BODY()

public:

	FORCEINLINE FStatusParameters() {}

	FORCEINLINE bool HasChanged(FStatusParameters otherStatus)
	{
		const bool stateChange = StateIndex != otherStatus.StateIndex;
		const bool stateFlagChange = PrimaryStateFlag != otherStatus.PrimaryStateFlag;
		const bool actionChange = ActionIndex != otherStatus.ActionIndex;
		return  stateChange || stateFlagChange || actionChange;
	}


	UPROPERTY(EditAnywhere)
	int StateIndex = -1;

	UPROPERTY(EditAnywhere)
	int ActionIndex = -1;

	UPROPERTY(EditAnywhere)
	int PrimaryStateFlag = 0;

	UPROPERTY(EditAnywhere)
	TArray<double> StateModifiers;

	UPROPERTY(EditAnywhere)
	TArray<double> ActionsModifiers;
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
	FVector ConstantLinearVelocity = FVector(0);

	UPROPERTY(SkipSerialization, EditAnywhere, BlueprintReadWrite, Category = "Velocity")
	FVector InstantLinearVelocity = FVector(0);

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

	FORCEINLINE FKinematicInfos(const  FVector InGravity, const  FKinematicInfos fromLastMove, const float inMass = 0)
	{
		Gravity = InGravity;
		InitialTransform = fromLastMove.FinalTransform;
		InitialVelocities = fromLastMove.FinalVelocities;
		InitialSurface = fromLastMove.FinalSurface;
		Mass = inMass;
	}

	FORCEINLINE FKinematicInfos(const FTransform fromTransform, const  FVelocity fromVelocity, const  FSurfaceInfos onSurface)
	{
		InitialTransform = fromTransform;
		InitialVelocities = fromVelocity;
		InitialSurface = onSurface;
	}


	FORCEINLINE void FromInitialValues(const FKinematicInfos& ref, bool copyFinals = false)
	{
		InitialSurface = ref.InitialSurface;
		InitialVelocities = ref.InitialVelocities;
		InitialTransform = ref.InitialTransform;
		if (copyFinals)
		{
			FinalSurface = ref.FinalSurface;
			FinalVelocities = ref.FinalVelocities;
			FinalTransform = ref.FinalTransform;

		}
		else
		{
			FinalSurface = FSurfaceInfos();
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




/// <summary>
/// The Data the client send to the server in order to be checked. also kept in the history for correction
/// </summary>
USTRUCT(BlueprintType)
struct FClientNetMoveCommand
{
	GENERATED_BODY()

public:

	FORCEINLINE FClientNetMoveCommand() {}

	FORCEINLINE FClientNetMoveCommand(double timeStamp, float deltaTime, FVector userMove, FKinematicInfos kinematicInfos, FStatusParameters controllerStatus = FStatusParameters())
	{
		TimeStamp = timeStamp;
		DeltaTime = deltaTime;
		userMoveInput = userMove;
		FromLocation = kinematicInfos.InitialTransform.GetLocation();
		ToLocation = kinematicInfos.FinalTransform.GetLocation();
		FromRotation = kinematicInfos.InitialTransform.Rotator();
		ToRotation = kinematicInfos.FinalTransform.Rotator();
		ToVelocity = kinematicInfos.FinalVelocities.ConstantLinearVelocity;
		WithVelocity = kinematicInfos.InitialVelocities.ConstantLinearVelocity;
		ControllerStatus = controllerStatus;
	}

	/// <summary>
	/// Get the offset of position, displacement made during this command.
	/// </summary>
	/// <returns></returns>
	FORCEINLINE FVector GetLocationOffset() const { return ToLocation - FromLocation; }

	/// <summary>
	/// Get the rotation offset of this command.
	/// </summary>
	/// <returns></returns>
	FORCEINLINE FQuat GetRotationOffset() const
	{
		auto finalRot = ToRotation.Quaternion();
		finalRot.EnforceShortestArcWith(FromRotation.Quaternion());
		auto rotOffset = FromRotation.Quaternion().Inverse() * finalRot;
		return rotOffset;
	}

	/// <summary>
	/// Get the difference of speed during the move
	/// </summary>
	/// <returns></returns>
	FORCEINLINE FVector GetAccelerationVector() const { return ToVelocity - WithVelocity; }


	/// <summary>
	/// Check if this command is dirty and have to be send over.
	/// </summary>
	/// <param name="otherCmd"></param>
	/// <returns></returns>
	FORCEINLINE bool HasChanged(FClientNetMoveCommand otherCmd, double minLocationOffset = 10, double minAngularOffset = 10, double velocityOffset = 10, FVector* debugValues = NULL)
	{
		double locationOffset = (ToLocation - otherCmd.ToLocation).Length();
		double angularOffset = FMath::RadiansToDegrees(ToRotation.Quaternion().AngularDistance(otherCmd.ToRotation.Quaternion()));
		double speedOffset = (WithVelocity - otherCmd.WithVelocity).Length();
		if (debugValues)
		{
			(*debugValues) = FVector(locationOffset, angularOffset, speedOffset);
		}
		return locationOffset > minLocationOffset || angularOffset >= minAngularOffset || speedOffset >= velocityOffset || ControllerStatus.HasChanged(otherCmd.ControllerStatus);
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	double TimeStamp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DeltaTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool CorrectionAckowledgement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 userMoveInput;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 FromLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 ToLocation;

	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 ToVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 WithVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FRotator FromRotation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FRotator ToRotation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FStatusParameters ControllerStatus;

};



/// <summary>
/// The Data send from server to client to correct him.
/// </summary>
USTRUCT(BlueprintType)
struct FServerNetCorrectionData
{
	GENERATED_BODY()

	FORCEINLINE FServerNetCorrectionData() {}

	FORCEINLINE FServerNetCorrectionData(double timeStamp, FKinematicInfos kinematicInfos, FHitResult* collision = nullptr)
	{
		TimeStamp = timeStamp;
		if (collision && collision->IsValidBlockingHit())
		{
			CollisionOccured = true;
			CollisionNormal = collision->Normal;
		}
		ToLocation = kinematicInfos.FinalTransform.GetLocation();
		WithVelocity = kinematicInfos.FinalVelocities.ConstantLinearVelocity;
		ToRotation = kinematicInfos.FinalTransform.GetRotation().Rotator();
	}

	/// <summary>
	/// Apply the correction where it should.
	/// </summary>
	/// <param name="moveHistory"></param>
	/// <returns></returns>
	FORCEINLINE bool ApplyCorrectionRecursive(TArray<FClientNetMoveCommand>& moveHistory, FClientNetMoveCommand& correctionResult)
	{
		if (TimeStamp == 0)
			return false;
		if (moveHistory.Num() <= 0)
			return false;
		const int index = moveHistory.IndexOfByPredicate([this](FClientNetMoveCommand histCmd)->bool {return histCmd.TimeStamp == TimeStamp; });
		if (!moveHistory.IsValidIndex(index))
			return false;
		for (int i = index - 1; i >= 0; i--)
			moveHistory.RemoveAt(i);
		moveHistory[0].ToLocation = ToLocation;
		moveHistory[0].ToRotation = ToRotation;
		moveHistory[0].WithVelocity = WithVelocity;
		moveHistory[0].ToVelocity = WithVelocity;


		for (int i = 1; i < moveHistory.Num(); i++)
		{
			FVector locOffset = moveHistory[i].GetLocationOffset();
			if (CollisionOccured)
			{
				const bool tryingGoThroughTheWall = FVector::DotProduct(locOffset, CollisionNormal) < 0;
				if (tryingGoThroughTheWall)
					locOffset = FVector::VectorPlaneProject(locOffset, CollisionNormal);
			}
			moveHistory[i].FromLocation = moveHistory[i - 1].ToLocation;
			moveHistory[i].ToLocation = moveHistory[i].FromLocation + locOffset;

			FQuat rotOffset = moveHistory[i].GetRotationOffset();
			moveHistory[i].FromRotation = moveHistory[i - 1].ToRotation;
			moveHistory[i].ToRotation = (moveHistory[i].FromRotation.Quaternion() * rotOffset).Rotator();

			FVector acceleration = moveHistory[i].GetAccelerationVector();
			moveHistory[i].WithVelocity = moveHistory[i - 1].ToVelocity;
			moveHistory[i].ToVelocity = moveHistory[i].WithVelocity + acceleration;
		}

		auto lastItem = moveHistory[moveHistory.Num() - 1];
		correctionResult = lastItem;
		return true;
	}


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	double TimeStamp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool CollisionOccured;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 CollisionNormal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 ToLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector_NetQuantize10 WithVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FRotator ToRotation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FStatusParameters ControllerStatus;
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

	UFUNCTION(BlueprintCallable, Category = "FInputEntry")
	static FVector GetAxisRelativeDirection(FVector2D input, FTransform transformRelative, FVector planeNormal)
	{
		FVector direction = FVector(0);
		FVector fwd = transformRelative.GetRotation().GetForwardVector();
		FVector rht = transformRelative.GetRotation().GetRightVector();
		if (planeNormal.Length() > 0 && planeNormal.Normalize())
		{
			fwd = FVector::VectorPlaneProject(fwd, planeNormal).GetSafeNormal();
			rht = FVector::VectorPlaneProject(rht, planeNormal).GetSafeNormal();
		}
		const FVector compositeRhs = rht * input.X;
		const FVector compositeFwd = fwd * input.Y;
		direction = compositeFwd + compositeRhs;
		return  direction;
	}
	

	UFUNCTION(BlueprintCallable, Category = "UInputEntryPool")
	static FInputEntry ReadInput(const UInputEntryPool* MyStructRef, FName key)
	{
		if (!MyStructRef)
			return {};
		return MyStructRef->ReadInput(key);
	}

	UFUNCTION(BlueprintCallable, Category = "UInputEntryPool")
	static FInputEntry ConsumeInput(UInputEntryPool* MyStructRef, FName key)
	{
		if (!MyStructRef)
			return {};
		return MyStructRef->ConsumeInput(key);
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

	UFUNCTION(BlueprintCallable, Category = "Surface|Debug")
	static void DrawDebugCircleOnSurface(const FHitResult MyStructRef, bool useImpact = false, float radius = 40, FColor color = FColor::White, float duration = 0, float thickness = 1, bool showAxis = false)
	{
		if (!MyStructRef.GetActor())
			return;
		FVector up = useImpact ? MyStructRef.ImpactNormal : MyStructRef.Normal;
		if (!up.Normalize())
			return;
		FVector right = up.Rotation().Quaternion().GetAxisY();
		FVector forward = FVector::CrossProduct(right, up);
		FVector::CreateOrthonormalBasis(forward, right, up);
		FVector hitPoint = MyStructRef.ImpactPoint + up * 0.01;
		if (showAxis)
		{
			UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetActor(), hitPoint, hitPoint + up * radius, (radius * 0.25), FColor::Blue, duration, thickness);
			UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetActor(), hitPoint, hitPoint + forward * (radius * 0.5), (radius * 0.25), FColor::Red, duration, thickness);
			UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetActor(), hitPoint, hitPoint + right * (radius * 0.5), (radius * 0.25), FColor::Green, duration, thickness);
		}
		UKismetSystemLibrary::DrawDebugCircle(MyStructRef.GetActor(), hitPoint, radius, 32,
			color, duration, thickness, right, forward);
	}


	template <typename T>
	UFUNCTION(BlueprintCallable, Category = "Common Objects")
	static std::enable_if_t<std::is_base_of_v<UObject, T>, T> GetObject(const TSoftObjectPtr<T> softObj)
	{
		if (!softObj.IsValid())
			return nullptr;
		return softObj.Get();
	}

	/// <summary>
	/// Return a rotation progressivelly turned toward desired look direction
	/// </summary>
	/// <param name="inRotation"></param>
	/// <param name="rotAxis"></param>
	/// <param name="desiredLookDirection"></param>
	/// <param name="rotationSpeed"></param>
	/// <param name="deltaTime"></param>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable, Category = "Transform Tools")
	static FQuat GetProgressiveRotation(const FQuat inRotation, const FVector rotAxis, const FVector desiredLookDirection, const float rotationSpeed, const float deltaTime)
	{
		FVector fwd = desiredLookDirection;
		FVector up = rotAxis;
		if (!fwd.Normalize())
			return inRotation;
		if (!up.Normalize())
			return inRotation;
		fwd = FVector::VectorPlaneProject(fwd, up);
		if (FVector::DotProduct(fwd.GetSafeNormal(), inRotation.Vector().GetSafeNormal()) <= -0.98f)
		{
			const FVector rgt = FVector::CrossProduct(up, fwd).GetSafeNormal();
			fwd += rgt * 0.1f;
		}
		fwd.Normalize();
		const FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
		const FQuat rotation = FQuat::Slerp(inRotation, fwdRot, FMath::Clamp(deltaTime * rotationSpeed, 0, 1));
		return rotation;
	}

	/// <summary>
	/// Linear interpolate a vector toward another with constant acceleration.
	/// </summary>
	/// <param name="fromVelocity"></param>
	/// <param name="toVelocity"></param>
	/// <param name="withAcceleration"></param>
	/// <param name="deltaTime"></param>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable, Category = "Transform Tools")
	static FVector AccelerateTo(const FVector fromVelocity, const FVector toVelocity, const float withAcceleration, const float deltaTime)
	{
		float trueAcceleration = withAcceleration;
		if (toVelocity.SquaredLength() > fromVelocity.SquaredLength())
			trueAcceleration = 1 / ((toVelocity.Length() * 0.01f) / withAcceleration);
		else if (toVelocity.SquaredLength() <= fromVelocity.SquaredLength())
			trueAcceleration = 1 / ((fromVelocity.Length() * 0.01f) / withAcceleration);
		else
			trueAcceleration = 0;
		const FVector endVel = FMath::Lerp(fromVelocity, toVelocity, FMath::Clamp(deltaTime * trueAcceleration, 0, 1));
		return  endVel;
	}

};

#pragma endregion