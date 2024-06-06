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
	FVector Axis = FVector(0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	float InputBuffer = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	TEnumAsByte<EInputEntryPhase> Phase = EInputEntryPhase::InputEntryPhase_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Input")
	float HeldDuration = 0;


	FORCEINLINE void Reset()
	{
		Axis = FVector(0);
		HeldDuration = 0;
		InputBuffer = 0;
		Phase = EInputEntryPhase::InputEntryPhase_None;
	}
};


/*
* Represent a pack of input entry, tracking inputs. Used locally only. not intended to be used remotely
*/
UCLASS(BlueprintType)
class MODULARCONTROLLER_API UInputEntryPool : public UObject
{
	GENERATED_BODY()

public:

	//The input pool
	TMap<FName, FInputEntry> _inputPool;

	//The input pool of the last frame
	TMap<FName, FInputEntry> _inputPool_last;



	/// <summary>
	/// Add input to the input pool. return true when added not replaced
	/// </summary>
	FORCEINLINE bool AddOrReplace(FName key, FInputEntry entry, const bool hold = false)
	{
		if (key.IsNone())
			return false;

		if (_inputPool.Contains(key))
		{
			entry.Phase = hold ? EInputEntryPhase::InputEntryPhase_Held : EInputEntryPhase::InputEntryPhase_Pressed;
			_inputPool[key] = entry;
		}
		else
		{
			entry.Phase = hold ? EInputEntryPhase::InputEntryPhase_Held : EInputEntryPhase::InputEntryPhase_Pressed;
			_inputPool.Add(key, entry);
		}

		return true;
	}

	/// <summary>
	/// Get input from the inputs pool
	/// </summary>
	FORCEINLINE FInputEntry ReadInput(const FName key) const
	{
		FInputEntry entry = FInputEntry();
		if (_inputPool_last.Contains(key))
		{
			entry.Nature = _inputPool_last[key].Nature;
			entry.Type = _inputPool_last[key].Type;
			entry.Phase = _inputPool_last[key].Phase;
			entry.Axis = _inputPool_last[key].Axis;
			entry.HeldDuration = _inputPool_last[key].HeldDuration;
		}
		else if (_inputPool.Contains(key))
		{
			entry.Nature = _inputPool[key].Nature;
			entry.Type = _inputPool[key].Type;
			entry.Phase = _inputPool[key].Phase;
			entry.Axis = _inputPool[key].Axis;
			entry.HeldDuration = _inputPool[key].HeldDuration;
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
	/// Update the inputs pool
	/// </summary>
	FORCEINLINE void UpdateInputs(float delta, const bool debug = false, UObject* worldContext = NULL)
	{
		//Update Existing
		for (auto& entry : _inputPool_last)
		{
			if (_inputPool_last[entry.Key].InputBuffer > 0)
			{
				auto inp = _inputPool_last[entry.Key];
				inp.InputBuffer -= delta;
				_inputPool_last[entry.Key] = inp;
			}
		}

		//New comers
		for (auto& entry : _inputPool)
		{
			if (!_inputPool_last.Contains(entry.Key))
			{
				auto input = entry.Value;
				input.HeldDuration = 0;
				_inputPool_last.Add(entry.Key, input);
			}
			else
			{
				auto input = entry.Value;
				_inputPool_last[entry.Key].Phase = input.Phase;
				_inputPool_last[entry.Key].HeldDuration = input.Phase == InputEntryPhase_Held ? delta + _inputPool_last[entry.Key].HeldDuration : 0;
				_inputPool_last[entry.Key].Axis = entry.Value.Axis;
				_inputPool_last[entry.Key].InputBuffer = input.InputBuffer;
			}
		}

		//Gones
		for (auto& entry : _inputPool_last)
		{
			if (!_inputPool.Contains(entry.Key))
			{
				if (entry.Value.Phase == InputEntryPhase_Released)
				{
					_inputPool_last[entry.Key].Reset();
				}
				else if (entry.Value.Phase != InputEntryPhase_None)
				{
					if (_inputPool_last[entry.Key].Type == InputEntryType_Buffered)
					{
						if (entry.Value.InputBuffer <= 0)
							_inputPool_last[entry.Key].Phase = EInputEntryPhase::InputEntryPhase_Released;
						else
							_inputPool_last[entry.Key].Phase = EInputEntryPhase::InputEntryPhase_Pressed;
						_inputPool_last[entry.Key].HeldDuration = 0;
					}
					else
					{
						_inputPool_last[entry.Key].Phase = EInputEntryPhase::InputEntryPhase_Released;
						_inputPool_last[entry.Key].HeldDuration = 0;
					}
				}
			}

			if (debug && worldContext)
			{
				const float bufferChrono = _inputPool_last[entry.Key].InputBuffer;
				const float activeDuration = _inputPool_last[entry.Key].HeldDuration;
				FColor debugColor;
				switch (_inputPool_last[entry.Key].Nature)
				{
				default:
					debugColor = FColor::White;
					break;
				case InputEntryNature_Axis:
					debugColor = FColor::Cyan;
					break;
				case InputEntryNature_Value:
					debugColor = FColor::Blue;
					break;
				}
				if (_inputPool_last[entry.Key].Phase == EInputEntryPhase::InputEntryPhase_None)
				{
					debugColor = FColor::Black;
				}
				UKismetSystemLibrary::PrintString(worldContext, FString::Printf(TEXT("Input: (%s), Nature: (%s), Phase: (%s), buffer: %f, Held: %f"), *entry.Key.ToString(), *UEnum::GetValueAsName<EInputEntryNature>(_inputPool_last[entry.Key].Nature).ToString(), *UEnum::GetValueAsName<EInputEntryPhase>(_inputPool_last[entry.Key].Phase).ToString(), bufferChrono
					, activeDuration), true, true, debugColor, 0, entry.Key);
			}
		}

		_inputPool.Empty();
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
	FORCEINLINE void UpdateSurfaceInfos(FTransform inTransform, const FHitResult selectedSurface, const float delta)
	{
		if (updateLock)
			return;

		updateLock = true;
		_surfaceHitResult = selectedSurface;
		_surfaceNormal = selectedSurface.Normal;

		//We're on the same surface
		if (_currentSurface.Get() != nullptr && selectedSurface.GetComponent() != nullptr && _currentSurface.Get() == selectedSurface.GetComponent() && !_currentSurface_Location.ContainsNaN())
		{
			isSurfaceSwitch = false;
			FTransform surfaceTransform = _currentSurface->GetComponentTransform();
			FVector look = surfaceTransform.TransformVector(_surfaceLocalLookDir);
			FVector pos = surfaceTransform.TransformPosition(_surfaceLocalHitPoint);

			//Velocity
			{
				//Linear Part
				FVector bodyVelocity = FVector(0);
				bodyVelocity = (selectedSurface.GetComponent()->GetComponentLocation() - _currentSurface_Location) / delta;

				//Angular part
				FQuat currentPl_quat = selectedSurface.GetComponent()->GetComponentRotation().Quaternion();
				FQuat lastPl_quat = _currentSurface_Rotation;
				lastPl_quat.EnforceShortestArcWith(currentPl_quat);
				FQuat pl_rotDiff = currentPl_quat * lastPl_quat.Inverse();
				float angle;
				FVector axis;
				pl_rotDiff.ToAxisAndAngle(axis, angle);
				angle /= delta;
				FVector dir, up, fwd;
				up = axis;
				fwd = FVector::VectorPlaneProject(inTransform.GetLocation() - _currentSurface->GetComponentLocation(), up).GetSafeNormal();
				dir = FVector::CrossProduct(up, fwd);
				dir.Normalize();
				double r = FVector::VectorPlaneProject(inTransform.GetLocation() - _currentSurface->GetComponentLocation(), up).Length() * 0.01;
				FVector rotVel = r * angle * dir;

				//Finally
				_surfaceLinearCompositeVelocity = bodyVelocity;// *1.2331;
				_surfaceAngularCompositeVelocity = rotVel;// *1.2331;
				_surfaceAngularCentripetalVelocity = -fwd * ((angle * angle) / r) * 0.0215;
			}

			//Orientation
			{
				FQuat targetQuat = selectedSurface.GetComponent()->GetComponentRotation().Quaternion();
				FQuat currentQuat = _currentSurface_Rotation;

				//Get Angular speed
				currentQuat.EnforceShortestArcWith(targetQuat);
				auto quatDiff = targetQuat * currentQuat.Inverse();
				FVector axis;
				float angle;
				quatDiff.ToAxisAndAngle(axis, angle);
				quatDiff = FQuat(axis, angle);
				_surfaceAngularVelocity = quatDiff;
			}
		}
		//we changed surfaces
		if (_currentSurface != selectedSurface.GetComponent())
		{
			Reset();
			isSurfaceSwitch = true;
		}
		_lastSurface = _currentSurface;
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
	/// Release the update lock
	/// </summary>
	FORCEINLINE void ReleaseLock()
	{
		if (!updateLock)
			return;
		updateLock = false;
	}

	/// <summary>
	/// Reset the surface infos
	/// </summary>
	FORCEINLINE void Reset()
	{
		_currentSurface = nullptr;
		_surfaceLinearCompositeVelocity = FVector(0);
		_surfaceAngularCompositeVelocity = FVector(0);
		_surfaceAngularCentripetalVelocity = FVector(0);
		_surfaceAngularVelocity = FQuat::Identity;
		_surfaceLocalHitPoint = FVector(0);
		_currentSurface_Location = FVector(NAN);
		_currentSurface_Rotation = FQuat::Identity;
		_surfaceLocalLookDir = FVector(0);
		ReleaseLock();
	}

	/// <summary>
	/// Consume the last evaluated linear velocity
	/// </summary>
	FORCEINLINE FVector ConsumeSurfaceLinearVelocity(bool linear = true, bool angular = true, bool centripetal = false)
	{
		FVector velocity = FVector(0);
		if (linear)
		{
			velocity += _surfaceLinearCompositeVelocity;
			_surfaceLinearCompositeVelocity = FVector(0);
		}
		if (angular)
		{
			velocity += _surfaceAngularCompositeVelocity * 100;
			_surfaceAngularCompositeVelocity = FVector(0);
		}
		if (centripetal)
		{
			velocity += _surfaceAngularCentripetalVelocity * 100;
			_surfaceAngularCentripetalVelocity = FVector(0);
		}
		return velocity;
	}

	/// <summary>
	/// Get the last evaluated linear velocity
	/// </summary>
	FORCEINLINE FVector GetSurfaceLinearVelocity(bool linear = true, bool angular = true, bool centripetal = false) const
	{
		FVector velocity = FVector(0);
		if (linear)
			velocity += _surfaceLinearCompositeVelocity;
		if (angular)
			velocity += _surfaceAngularCompositeVelocity * 100;
		if (centripetal)
			velocity += _surfaceAngularCentripetalVelocity * 100;
		return velocity;
	}

	/// <summary>
	/// Get the last evaluated angular velocity
	/// </summary>
	FORCEINLINE FQuat GetSurfaceAngularVelocity(bool consume = false)
	{
		FQuat value = _surfaceAngularVelocity;
		if (consume)
		{
			_surfaceAngularVelocity = FQuat::Identity;
		}
		return value;
	}

	/// <summary>
	/// Get the last evaluated surface normal
	/// </summary>
	FORCEINLINE FVector GetSurfaceNormal() const { return  _surfaceNormal; }

	/// <summary>
	/// Get surface primitive
	/// </summary>
	FORCEINLINE UPrimitiveComponent* GetSurfacePrimitive() const { return  _currentSurface.Get(); }

	/// <summary>
	/// Get last surface primitive
	/// </summary>
	FORCEINLINE UPrimitiveComponent* GetLastSurfacePrimitive() const { return  _lastSurface.Get(); }

	/// <summary>
	/// Get surface hit result data
	/// </summary>
	FORCEINLINE FHitResult GetHitResult() const { return  _surfaceHitResult; }

	/// <summary>
	/// Get if the surface was changed
	/// </summary>
	FORCEINLINE bool HadChangedSurface() const { return  isSurfaceSwitch; }

	/// <summary>
	/// Get if the surface we just landed on this surface
	/// </summary>
	FORCEINLINE bool HadLandedOnSurface() const { return  _currentSurface && !_lastSurface; }

	/// <summary>
	/// Get if the surface we just took off this surface
	/// </summary>
	FORCEINLINE bool HadTookOffSurface() const { return  !_currentSurface && _lastSurface; }

public:

	//the surface hit raycast
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FHitResult _surfaceHitResult;

protected:
	//------------------------------------------------------------------------------------------

	//the current surface
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	TSoftObjectPtr<UPrimitiveComponent> _currentSurface;

	//the last surface
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	TSoftObjectPtr<UPrimitiveComponent> _lastSurface;

	//the surface linear velocity
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceLinearCompositeVelocity = FVector(0);

	//the surface angular velocity
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceAngularCompositeVelocity = FVector(0);

	//the surface angular centripedal velocity
	UPROPERTY(SkipSerialization, VisibleAnywhere, BlueprintReadOnly, Category = "Surface|Surface Infos")
	FVector _surfaceAngularCentripetalVelocity = FVector(0);

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

	//prevent the surface from being updated twice during a frame.
	bool updateLock;

	//Enabled if the surface had been switched during this frame.
	bool isSurfaceSwitch;
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
	UAnimMontage* Montage = nullptr;

	/// <summary>
	/// The Animation Montages section to play
	/// </summary>
	UPROPERTY(EditAnywhere, Category = "Action|Types|Montage")
	FName MontageSection = "";
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

	FORCEINLINE bool HasChanged(FStatusParameters otherStatus) const
	{
		const bool stateChange = StateIndex != otherStatus.StateIndex;
		const bool stateFlagChange = PrimaryStateFlag != otherStatus.PrimaryStateFlag;
		const bool actionChange = ActionIndex != otherStatus.ActionIndex;
		const bool actionFlagChange = PrimaryActionFlag != otherStatus.PrimaryActionFlag;
		return  stateChange || stateFlagChange || actionChange || actionFlagChange;
	}


	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	int StateIndex = -1;

	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	int ActionIndex = -1;

	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	int PrimaryStateFlag = 0;

	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	int PrimaryActionFlag = 0;

	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	FVector_NetQuantize10 StateModifiers1;

	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	FVector_NetQuantize10 StateModifiers2;

	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	FVector_NetQuantize10 ActionsModifiers1;

	UPROPERTY(EditAnywhere, Category = "StatusParameters")
	FVector_NetQuantize10 ActionsModifiers2;
};


#pragma endregion



#pragma region MovementInfosAndReplication


//Represent a single kinematic linear condition
USTRUCT(BlueprintType)
struct FLinearKinematicCondition
{
	GENERATED_BODY()

public:

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



	//Set the referential movement (usually the surface the controller is on)
	FORCEINLINE void SetReferentialMovement(const FVector movement, const float delta, const float acceleration = -1)
	{
		const double acc = acceleration >= 0 ? acceleration : 1 / delta;
		if (acc <= 0)
		{
			refAcceleration = FVector(0);
			refVelocity = FVector(0);
			return;
		}
		const double t = FMath::Clamp(acc * (1 / (3 * delta)), 0, 1 / delta);
		const FVector v = movement;
		const FVector v0 = refVelocity;
		FVector a = FVector(0);
		a.X = (v.X - v0.X) * t;
		a.Y = (v.Y - v0.Y) * t;
		a.Z = (v.Z - v0.Z) * t;
		refAcceleration = a;
		refVelocity = a * delta + v0;
	}

	//Add a composite movement. useful to match a certain speed;
	FORCEINLINE void AddCompositeMovement(const FVector movement, const float acceleration = -1, int index = -1)
	{
		if (index < 0)
		{
			bool replaced = false;
			for (int i = 0; i < CompositeMovements.Num(); i++)
			{
				if (CompositeMovements[i].W == 0)
				{
					CompositeMovements[i] = FVector4d(movement.X, movement.Y, movement.Z, acceleration);
					replaced = true;
				}
			}
			if (!replaced)
			{
				CompositeMovements.Add(FVector4d(movement.X, movement.Y, movement.Z, acceleration));
			}
		}
		else if (CompositeMovements.IsValidIndex(index))
		{
			CompositeMovements[index] = FVector4d(movement.X, movement.Y, movement.Z, acceleration);
		}
		else
		{
			for (int i = CompositeMovements.Num(); i <= index; i++)
			{
				if (i == index)
					CompositeMovements.Add(FVector4d(movement.X, movement.Y, movement.Z, acceleration));
				else
					CompositeMovements.Add(FVector4d(0, 0, 0, 0));
			}
		}
	}

	//Remove a composite movement at index.
	FORCEINLINE bool RemoveCompositeMovement(int index)
	{
		if (CompositeMovements.IsValidIndex(index))
		{
			CompositeMovements.RemoveAt(index);
			return true;
		}
		else
			return false;
	}


	//Compute an acceleration from this condition leading to the desired velocity
	FORCEINLINE FVector GetAccelerationFromVelocity(FVector desiredVelocity, double deltaTime, bool onlyContribution = false)
	{
		FVector velocityDiff = desiredVelocity - Velocity;
		if (onlyContribution && desiredVelocity.Length() < Velocity.Length())
			velocityDiff = desiredVelocity * deltaTime;
		return velocityDiff / deltaTime;
	}

	//Evaluate future movement conditions base on the delta time.
	FORCEINLINE FLinearKinematicCondition GetFinalCondition(double deltaTime)
	{
		ComputeCompositeMovement(deltaTime);
		FLinearKinematicCondition finalCondition = FLinearKinematicCondition();
		//X part
		double x = 0.5 * Acceleration.X * (deltaTime * deltaTime) + Velocity.X * deltaTime + Position.X;
		double velx = Acceleration.X * deltaTime + Velocity.X;
		//Y part
		double y = 0.5 * Acceleration.Y * (deltaTime * deltaTime) + Velocity.Y * deltaTime + Position.Y;
		double vely = Acceleration.Y * deltaTime + Velocity.Y;
		//Z part
		double z = 0.5 * Acceleration.Z * (deltaTime * deltaTime) + Velocity.Z * deltaTime + Position.Z;
		double velz = Acceleration.Z * deltaTime + Velocity.Z;

		finalCondition.Position = FVector(x, y, z);
		finalCondition.Velocity = FVector(velx, vely, velz);
		finalCondition.Acceleration = Acceleration;
		finalCondition.SnapDisplacement = SnapDisplacement;
		finalCondition.Time = Time + deltaTime;
		finalCondition.refAcceleration = refAcceleration;
		finalCondition.refVelocity = refVelocity;
		return finalCondition;
	}

	//Evaluate future movement conditions base both the delta time and a targeted position.
	FORCEINLINE FLinearKinematicCondition GetFinalFromPosition(FVector targetPosition, double deltaTime, bool affectAcceleration = false)
	{
		ComputeCompositeMovement(deltaTime);
		FLinearKinematicCondition fixedCondition = FLinearKinematicCondition();
		fixedCondition.Position = targetPosition;
		fixedCondition.Acceleration = Acceleration;
		fixedCondition.SnapDisplacement = SnapDisplacement;
		fixedCondition.Time = Time + deltaTime;
		fixedCondition.refAcceleration = refAcceleration;
		fixedCondition.refVelocity = refVelocity;

		//Velocity
		{
			//X part
			double velX = ((2 * (targetPosition.X - Position.X)) / deltaTime) - Velocity.X;
			//Y part
			double velY = ((2 * (targetPosition.Y - Position.Y)) / deltaTime) - Velocity.Y;
			//Z part
			double velZ = ((2 * (targetPosition.Z - Position.Z)) / deltaTime) - Velocity.Z;

			fixedCondition.Velocity = FVector(velX, velY, velZ);
		}

		//Acceleration
		if (affectAcceleration)
		{
			//X part
			double accX = (fixedCondition.Velocity.X - Velocity.X) / deltaTime;
			//Y part
			double accY = (fixedCondition.Velocity.Y - Velocity.Y) / deltaTime;
			//Z part
			double accZ = (fixedCondition.Velocity.Z - Velocity.Z) / deltaTime;

			fixedCondition.Acceleration = FVector(accX, accY, accZ);
		}

		return fixedCondition;
	}

protected:

	//Compute composite movement to take them in to account when moving.
	FORCEINLINE void ComputeCompositeMovement(const float delta)
	{
		//Referential
		const FVector relativeVelocity = Velocity - refVelocity;
		Acceleration += refAcceleration;

		if (CompositeMovements.IsEmpty())
			return;

		for (int i = CompositeMovements.Num() - 1; i >= 0; i--)
		{
			const auto moveParam = CompositeMovements[i];
			const FVector movement = FVector(moveParam.X, moveParam.Y, moveParam.Z);
			const double acceleration = moveParam.W >= 0 ? moveParam.W : 1 / delta;
			if (acceleration <= 0)
				continue;
			const double t = FMath::Clamp(acceleration * (1 / (3 * delta)), 0, 1 / delta);
			const FVector v = movement;
			const FVector v0 = relativeVelocity;
			FVector a = FVector(0);
			a.X = (v.X - v0.X) * t;
			a.Y = (v.Y - v0.Y) * t;
			a.Z = (v.Z - v0.Z) * t;
			Acceleration += a;
		}
	}
};

//Represent a single kinematic angular condition
USTRUCT(BlueprintType)
struct FAngularKinematicCondition
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
	FORCEINLINE FQuat GetAngularSpeedQuat(float time = 1) const
	{
		const FVector axis = RotationSpeed.GetSafeNormal();
		const float angle = FMath::DegreesToRadians(FMath::Clamp(RotationSpeed.Length() * time, 0, 360));
		const float halfTetha = angle * 0.5;
		const float sine = FMath::Sin(halfTetha);
		const float cosine = FMath::Cos(halfTetha);
		FQuat q = FQuat(axis.X * sine, axis.Y * sine, axis.Z * sine, cosine);
		return q;
	}

	//Evaluate future movement conditions base on the delta time.
	FORCEINLINE FAngularKinematicCondition GetFinalCondition(double deltaTime) const
	{
		FAngularKinematicCondition finalCondition = FAngularKinematicCondition();

		//X part
		double velx = AngularAcceleration.X * deltaTime + RotationSpeed.X;
		//Y part
		double vely = AngularAcceleration.Y * deltaTime + RotationSpeed.Y;
		//Z part
		double velz = AngularAcceleration.Z * deltaTime + RotationSpeed.Z;

		finalCondition.RotationSpeed = FVector(velx, vely, velz);
		const FQuat angularSpeed = finalCondition.GetAngularSpeedQuat(deltaTime);
		finalCondition.Orientation = Orientation * angularSpeed;
		finalCondition.AngularAcceleration = AngularAcceleration;
		finalCondition.Time = Time + deltaTime;


		return finalCondition;
	}

};


//Represent the kinematic conditions of an object
USTRUCT(BlueprintType)
struct FKinematicComponents
{
	GENERATED_BODY()

public:

	FORCEINLINE FKinematicComponents()
	{
	}

	FORCEINLINE FKinematicComponents(FLinearKinematicCondition linearCond, FAngularKinematicCondition angularCond)
	{
		LinearKinematic = linearCond;
		AngularKinematic = angularCond;
	}

	FORCEINLINE FKinematicComponents FromComponent(FKinematicComponents fromComponent, double withDelta)
	{
		LinearKinematic = fromComponent.LinearKinematic.GetFinalCondition(withDelta);
		AngularKinematic = fromComponent.AngularKinematic.GetFinalCondition(withDelta);
		return FKinematicComponents(LinearKinematic, AngularKinematic);
	}

	FORCEINLINE FKinematicComponents FromComponent(FKinematicComponents fromComponent, FVector linearAcceleration, double withDelta)
	{
		fromComponent.LinearKinematic.Acceleration = linearAcceleration;
		LinearKinematic = fromComponent.LinearKinematic.GetFinalCondition(withDelta);
		AngularKinematic = fromComponent.AngularKinematic.GetFinalCondition(withDelta);
		return FKinematicComponents(LinearKinematic, AngularKinematic);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FLinearKinematicCondition LinearKinematic = FLinearKinematicCondition();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinematics")
	FAngularKinematicCondition AngularKinematic = FAngularKinematicCondition();


	//Get the rotation from angular kinematic.
	FORCEINLINE FQuat GetRotation() const { return AngularKinematic.Orientation; }
};


// The result of a processed state or action
USTRUCT(BlueprintType)
struct FControllerStatus
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FKinematicComponents Kinematics = FKinematicComponents();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FStatusParameters ControllerStatus = FStatusParameters();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FVector MoveInput = FVector(0);

	//X= surface friction, Y=Drag, Z= Bounciness
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult")
	FVector CustomPhysicProperties = FVector(-1);

	//The current surface the controller is on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcessResult", SkipSerialization)
	FSurfaceInfos ControllerSurface = FSurfaceInfos();
};


// The result of a check on state or action
USTRUCT(BlueprintType)
struct FControllerCheckResult
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



#pragma region Extensions

//Extension methods for structures
UCLASS(BlueprintType)
class UStructExtensions : public UObject
{
	GENERATED_BODY()

public:

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


	UFUNCTION(BlueprintCallable, Category = "FSurfaceInfos")
	static FVector GetSurfaceLinearVelocity(const FSurfaceInfos MyStructRef, bool linear = true, bool angular = true)
	{
		return MyStructRef.GetSurfaceLinearVelocity(linear, angular);
	}

	UFUNCTION(BlueprintCallable, Category = "FSurfaceInfos")
	static FQuat GetSurfaceAngularVelocity(FSurfaceInfos MyStructRef)
	{
		return MyStructRef.GetSurfaceAngularVelocity();
	}

	UFUNCTION(BlueprintCallable, Category = "FSurfaceInfos")
	static FHitResult GetSurfaceHitInfos(const FSurfaceInfos MyStructRef)
	{
		return MyStructRef.GetHitResult();
	}


	UFUNCTION(BlueprintCallable, Category = "KinematicOperations")
	static FVector GetVelocityMatchingAcceleration(FVector desiredVelocity, FVector currentVelocity, bool reduceIfMore = false)
	{
		FVector diff = desiredVelocity - currentVelocity;
		if (currentVelocity.Length() <= 0)
			return diff;
		float scale = desiredVelocity.ProjectOnToNormal(currentVelocity.GetSafeNormal()).Length() / currentVelocity.Length();
		if (!reduceIfMore && scale > 0 && FVector::DotProduct(desiredVelocity, currentVelocity) > 0)
			return FVector(0);
		return diff;
	}


	//Get surface friction (X), surface bounciness (Y)
	UFUNCTION(BlueprintCallable, Category = "Surface|Physic")
	static FVector GetSurfacePhysicProperties(const FHitResult MyStructRef)
	{
		if (!MyStructRef.GetActor())
			return MyStructRef.GetComponent() ? FVector(1, 0, 0) : FVector(0);
		if (!MyStructRef.PhysMaterial.IsValid())
			return FVector(1, 0, 0);

		return FVector(MyStructRef.PhysMaterial->Friction, MyStructRef.PhysMaterial->Restitution, 0);
	}

	UFUNCTION(BlueprintCallable, Category = "Surface|Debug")
	static void DrawDebugCircleOnSurface(const FHitResult MyStructRef, bool useImpact = false, float radius = 40, FColor color = FColor::White, float duration = 0, float thickness = 1, bool showAxis = false)
	{
		if (!MyStructRef.GetComponent())
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
			UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + up * radius, (radius * 0.25), FColor::Blue, duration, thickness);
			UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + forward * (radius * 0.5), (radius * 0.25), FColor::Red, duration, thickness);
			UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + right * (radius * 0.5), (radius * 0.25), FColor::Green, duration, thickness);
		}
		UKismetSystemLibrary::DrawDebugCircle(MyStructRef.GetComponent(), hitPoint, radius, 32,
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


	UFUNCTION(BlueprintCallable, Category = "Transform Tools")
	static double GetFrictionAcceleration(const FVector normal, const FVector force, const double mass, const double frictionConst)
	{
		FVector n = normal;
		if (n.Normalize() && mass > 0)
		{
			FVector f = force.ProjectOnToNormal(n) * frictionConst;
			if ((n | f) < 0)
			{
				f *= -1;
			}
			f /= mass;
			return f.Length();
		}

		return 0;
	}


	/// <summary>
	/// Get the linear acceleration vector to match the target speed with an acceleration and deceleration.
	/// </summary>
	/// <param name="initialKinematic">The initial kinematic conditions</param>
	/// <param name="targetSpeed">The targeted speed</param>
	/// <param name="withAcceleration">The acceleration scalar in case target speed is greater than current speed</param>
	/// <param name="withDeceleration">The deceleration scalar in case target speed is less than current speed</param>
	/// <param name="deltaTime">the time delta</param>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable, Category = "Transform Tools")
	static FVector GetLinearAccelerationTo(const FLinearKinematicCondition initialKinematic, const FVector targetSpeed, const float withAcceleration, const float withDeceleration, const float deltaTime)
	{
		float trueAcceleration = withAcceleration;
		const FVector velocity = initialKinematic.Velocity;
		if (velocity.SquaredLength() > targetSpeed.SquaredLength())
			trueAcceleration = withDeceleration;

		const double t = FMath::Clamp(trueAcceleration * (1 / (3 * deltaTime)), 0, 1 / deltaTime);
		const FVector v = targetSpeed;
		const FVector v0 = velocity;
		FVector a = FVector(0);
		a.X = (v.X - v0.X) * t;
		a.Y = (v.Y - v0.Y) * t;
		a.Z = (v.Z - v0.Z) * t;

		return a;
	}


	//Turn toward a direction.
	UFUNCTION(BlueprintCallable, Category = "Transform Tools")
	static FAngularKinematicCondition LookAt(const FAngularKinematicCondition startCondition, const FVector direction, const float withSpeed, const float deltaTime)
	{
		FAngularKinematicCondition finalAngular = startCondition;
		FVector lookDir = direction;

		if (lookDir.Normalize())
		{
			FQuat orientTarget = lookDir.ToOrientationQuat();
			orientTarget.EnforceShortestArcWith(startCondition.Orientation);
			FQuat diff = startCondition.Orientation.Inverse() * orientTarget;
			float rotSpeed;
			FVector rotAxis;
			diff.ToAxisAndAngle(rotAxis, rotSpeed);
			const float limitedSpeed = FMath::Clamp(withSpeed, 0, 1 / deltaTime);
			finalAngular.RotationSpeed = rotAxis * FMath::RadiansToDegrees(rotSpeed) * limitedSpeed;
		}
		else if (startCondition.RotationSpeed.SquaredLength() > 0)
		{
			finalAngular.AngularAcceleration = -startCondition.RotationSpeed / (deltaTime * 4);
		}

		return finalAngular;
	}


	//Compute the final velocities of two colliding objects A and B, and return true if the operation succeeded.
	static bool ComputeCollisionVelocities(const FVector initialVelA, const FVector initialVelB, const FVector colNornal, const double massA, const double massB, const double bounceCoef
		, FVector& finalA, FVector& finalB)
	{
		FVector n = colNornal;
		if (!n.Normalize())
			return false;
		finalA = FVector::VectorPlaneProject(initialVelA, n);
		finalB = FVector::VectorPlaneProject(initialVelB, n);
		const FVector Va1 = initialVelA.ProjectOnToNormal(n);
		const FVector Vb1 = initialVelB.ProjectOnToNormal(n);
		const double cfa = bounceCoef * massA;
		const double cfb = bounceCoef * massB;
		const double massSum = massA + massB;
		const FVector Va2 = ((massA - cfb) / massSum) * Va1 + ((massB + cfb) / massSum) * Vb1;
		const FVector Vb2 = ((massB - cfa) / massSum) * Vb1 + ((massA + cfa) / massSum) * Va1;
		finalA += Va2;
		finalB += Vb2;
		return true;
	}

};

#pragma endregion