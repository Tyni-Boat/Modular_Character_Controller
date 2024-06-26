// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "CommonTypes.h"

#include "ToolsLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/NetSerialization.h"
#include "Kismet/KismetSystemLibrary.h"


#pragma region Inputs


FInputEntry::FInputEntry()
{
}

void FInputEntry::Reset()
{
	Axis = FVector(0);
	HeldDuration = 0;
	InputBuffer = 0;
	Phase = EInputEntryPhase::InputEntryPhase_None;
}



bool UInputEntryPool::AddOrReplace(FName key, FInputEntry entry, const bool hold)
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

FInputEntry UInputEntryPool::ReadInput(const FName key, bool consume)
{
	FInputEntry entry = FInputEntry();
	if (_inputPool_last.Contains(key))
	{
		entry.Nature = _inputPool_last[key].Nature;
		entry.Type = _inputPool_last[key].Type;
		if (consume && entry.Type == EInputEntryType::Buffered)
		{
			_inputPool_last[key].InputBuffer = 0;
		}
		entry.Phase = _inputPool_last[key].Phase;
		entry.Axis = _inputPool_last[key].Axis;
		entry.HeldDuration = _inputPool_last[key].HeldDuration;
	}
	else
	{
		entry.Nature = EInputEntryNature::InputEntryNature_Button;
		entry.Type = EInputEntryType::Simple;
		entry.Phase = EInputEntryPhase::InputEntryPhase_None;
		entry.Axis = FVector(0);
	}

	return entry;
}

void UInputEntryPool::UpdateInputs(float delta, const bool debug, UObject* worldContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UpdateInputs");

	//Update Existing
	for (auto& entry : _inputPool_last)
	{
		if (_inputPool_last[entry.Key].InputBuffer > 0)
		{
			_inputPool_last[entry.Key].InputBuffer -= delta;
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
				if (_inputPool_last[entry.Key].Type == EInputEntryType::Buffered)
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


#pragma endregion


#pragma region Surface and Zones

FSurfaceInfos::FSurfaceInfos()
{
}


void FSurfaceInfos::UpdateSurfaceInfos(FTransform inTransform, const FHitResult selectedSurface, const float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UpdateSurfaceInfos");
	if (updateLock)
		return;

	updateLock = true;
	_surfaceHitResult = selectedSurface;
	_surfaceNormal = selectedSurface.Normal;

	//We're on the same surface
	if (_currentSurface.IsValid() && selectedSurface.Component.IsValid() && _currentSurface == selectedSurface.Component && !_currentSurface_Location.ContainsNaN())
	{
		isSurfaceSwitch = false;
		//FTransform surfaceTransform = _currentSurface->GetComponentTransform();
		//FVector look = surfaceTransform.TransformVector(_surfaceLocalLookDir);
		//FVector pos = surfaceTransform.TransformPosition(_surfaceLocalHitPoint);

		//Velocity
		{
			//Linear Part
			FVector bodyVelocity = FVector(0);
			bodyVelocity = (selectedSurface.Component->GetComponentLocation() - _currentSurface_Location) / delta;

			//Angular part
			FQuat currentPl_quat = selectedSurface.Component->GetComponentRotation().Quaternion();
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
			FQuat targetQuat = selectedSurface.Component->GetComponentRotation().Quaternion();
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
	if (_currentSurface != selectedSurface.Component)
	{
		Reset();
		isSurfaceSwitch = true;
	}
	_lastSurface = _currentSurface;
	_currentSurface = selectedSurface.Component;
	if (_currentSurface.IsValid())
	{
		auto surfaceTransform = _currentSurface->GetComponentTransform();
		_surfaceLocalLookDir = surfaceTransform.InverseTransformVector(inTransform.GetRotation().Vector());
		_surfaceLocalHitPoint = surfaceTransform.InverseTransformPosition(inTransform.GetLocation());
		_currentSurface_Location = _currentSurface->GetComponentLocation();
		_currentSurface_Rotation = _currentSurface->GetComponentRotation().Quaternion();
	}
}

void FSurfaceInfos::ReleaseLock()
{
	if (!updateLock)
		return;
	updateLock = false;
}

void FSurfaceInfos::Reset()
{
	_currentSurface.Reset();
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

FVector FSurfaceInfos::ConsumeSurfaceLinearVelocity(bool linear, bool angular, bool centripetal)
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

FVector FSurfaceInfos::GetSurfaceLinearVelocity(bool linear, bool angular, bool centripetal) const
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

FQuat FSurfaceInfos::GetSurfaceAngularVelocity(bool consume)
{
	FQuat value = _surfaceAngularVelocity;
	if (consume)
	{
		_surfaceAngularVelocity = FQuat::Identity;
	}
	return value;
}

FVector FSurfaceInfos::GetSurfaceNormal() const
{
	return  _surfaceNormal;
}

UPrimitiveComponent* FSurfaceInfos::GetSurfacePrimitive() const
{
	return  _currentSurface.Get();
}

UPrimitiveComponent* FSurfaceInfos::GetLastSurfacePrimitive() const
{
	return  _lastSurface.Get();
}

FHitResult FSurfaceInfos::GetHitResult() const
{
	return  _surfaceHitResult;
}

bool FSurfaceInfos::HadChangedSurface() const
{
	return  isSurfaceSwitch;
}

bool FSurfaceInfos::HadLandedOnSurface() const
{
	return  _currentSurface.IsValid() && !_lastSurface.IsValid();
}

bool FSurfaceInfos::HadTookOffSurface() const
{
	return  !_currentSurface.IsValid() && _lastSurface.IsValid();
}





FSurfaceTrackData::FSurfaceTrackData()
{
}

bool FSurfaceTrackData::UpdateTracking(float deltaTime)
{
	FVector linearVelocity = FVector(0);
	FVector angularVelocity = FVector(0);
	bool validSurface = false;

	if (TrackedComponent.IsValid())
	{
		validSurface = true;

		//Linear Part
		linearVelocity = (TrackedComponent->GetComponentLocation() - _lastPosition) / deltaTime;

		//Angular
		const FQuat targetQuat = TrackedComponent->GetComponentRotation().Quaternion();
		FQuat currentQuat = _lastRotation;

		//Get Angular speed
		currentQuat.EnforceShortestArcWith(targetQuat);
		const FQuat quatDiff = targetQuat * currentQuat.Inverse();
		FVector axis;
		float angle;
		quatDiff.ToAxisAndAngle(axis, angle);
		axis.Normalize();
		angularVelocity = axis * angle / deltaTime;
	}

	LinearVelocity = linearVelocity;
	AngularVelocity = angularVelocity;
	return validSurface;
}


FVector FSurfaceTrackData::GetVelocityAt(const FVector point) const
{
	if (!TrackedComponent.IsValid())
		return LinearVelocity;

	//Angular part
	const FVector rotationAxis = AngularVelocity.GetSafeNormal();
	FVector radiusDirection = FVector::VectorPlaneProject(point - TrackedComponent->GetComponentLocation(), rotationAxis).GetSafeNormal();
	FVector tangentialDirection = FVector::CrossProduct(rotationAxis, radiusDirection);
	tangentialDirection.Normalize();
	double r = FVector::VectorPlaneProject(point - TrackedComponent->GetComponentLocation(), rotationAxis).Length() * 0.01;
	const double angle = AngularVelocity.Length();
	const FVector rotVel = r * angle * tangentialDirection;
	const FVector centripetal = -radiusDirection * ((angle * angle) / r) * 0.0215;

	//Finally
	return LinearVelocity + rotVel + centripetal;
}



FSurface::FSurface()
{
}

#pragma endregion


#pragma region States and Actions


FActionInfos::FActionInfos() {}

void FActionInfos::Init(FVector timings, float coolDown, int repeatCount)
{
	Reset(coolDown);
	_startingDurations = timings;
	_repeatCount = repeatCount;
	_remainingActivationTimer = _startingDurations.X + _startingDurations.Y + _startingDurations.Z;
}

double FActionInfos::GetRemainingActivationTime() const
{
	return _remainingActivationTimer;
}

double FActionInfos::GetRemainingCoolDownTime() const
{
	return _cooldownTimer;
}

double FActionInfos::GetNormalizedTime(EActionPhase phase) const
{
	switch (phase)
	{
		case ActionPhase_Undetermined: return 0;
		case ActionPhase_Anticipation:
			if (_remainingActivationTimer >= (_startingDurations.Y + _startingDurations.Z))
				return  1 - ((_remainingActivationTimer - (_startingDurations.Y + _startingDurations.Z)) / _startingDurations.X);
			return 0;
		case ActionPhase_Active:
			if (_remainingActivationTimer < _startingDurations.Z) return 0;
			if (_remainingActivationTimer > (_startingDurations.Y + _startingDurations.Z)) return 1;
			return 1 - ((_remainingActivationTimer - _startingDurations.Z) / _startingDurations.Y);
		case ActionPhase_Recovery:
			if (_remainingActivationTimer > _startingDurations.Z) return 1;
			return 1 - _remainingActivationTimer / _startingDurations.Z;
	}
	return 0;
}

void FActionInfos::SkipTimeToPhase(EActionPhase phase)
{
	switch (phase)
	{
		case ActionPhase_Anticipation:
			_remainingActivationTimer = _startingDurations.X + _startingDurations.Y + _startingDurations.Z;
			break;
		case ActionPhase_Active:
			_remainingActivationTimer = _startingDurations.Y + _startingDurations.Z;
			break;
		case ActionPhase_Recovery:
			_remainingActivationTimer = _startingDurations.Z;
			break;
	}
}

void FActionInfos::Update(float deltaTime)
{
	if (_remainingActivationTimer >= 0)
	{
		_remainingActivationTimer -= deltaTime;

		if (_remainingActivationTimer > (_startingDurations.Y + _startingDurations.Z))
		{
			if (CurrentPhase != ActionPhase_Anticipation)
			{
				CurrentPhase = ActionPhase_Anticipation;
			}
		}
		else if (_remainingActivationTimer > _startingDurations.Z && _remainingActivationTimer <= (_startingDurations.Y + _startingDurations.Z))
		{
			if (CurrentPhase != ActionPhase_Active)
			{
				CurrentPhase = ActionPhase_Active;
			}
		}
		else
		{
			if (CurrentPhase != ActionPhase_Recovery)
			{
				CurrentPhase = ActionPhase_Recovery;
			}
		}
	}
	else
	{
		CurrentPhase = EActionPhase::ActionPhase_Undetermined;
		if (_cooldownTimer > 0)
		{
			_cooldownTimer -= deltaTime;
		}
	}
}

void FActionInfos::Reset(float coolDown)
{
	_cooldownTimer = coolDown;
	_remainingActivationTimer = 0;
	_repeatCount = 0;
	CurrentPhase = EActionPhase::ActionPhase_Undetermined;
}




FActionMotionMontage::FActionMotionMontage()
{}



FStatusParameters::FStatusParameters()
{}

bool FStatusParameters::HasChanged(FStatusParameters otherStatus) const
{
	const bool stateChange = StateIndex != otherStatus.StateIndex;
	const bool stateFlagChange = PrimaryStateFlag != otherStatus.PrimaryStateFlag;
	const bool actionChange = ActionIndex != otherStatus.ActionIndex;
	const bool actionFlagChange = PrimaryActionFlag != otherStatus.PrimaryActionFlag;
	return  stateChange || stateFlagChange || actionChange || actionFlagChange;
}

#pragma endregion


#pragma region MovementInfosAndReplication

FLinearKinematicCondition::FLinearKinematicCondition()
{}

void FLinearKinematicCondition::SetReferentialMovement(const FVector movement, const float delta, const float acceleration)
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

void FLinearKinematicCondition::AddCompositeMovement(const FVector movement, const float acceleration, int index)
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

bool FLinearKinematicCondition::RemoveCompositeMovement(int index)
{
	if (CompositeMovements.IsValidIndex(index))
	{
		CompositeMovements.RemoveAt(index);
		return true;
	}
	else
		return false;
}

FVector FLinearKinematicCondition::GetAccelerationFromVelocity(FVector desiredVelocity, double deltaTime, bool onlyContribution) const
{
	FVector velocityDiff = desiredVelocity - Velocity;
	if (onlyContribution && desiredVelocity.Length() < Velocity.Length())
		velocityDiff = desiredVelocity * deltaTime;
	return velocityDiff / deltaTime;
}

FLinearKinematicCondition FLinearKinematicCondition::GetFinalCondition(double deltaTime)
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

FLinearKinematicCondition FLinearKinematicCondition::GetFinalFromPosition(FVector targetPosition, double deltaTime, bool affectAcceleration)
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

void FLinearKinematicCondition::ComputeCompositeMovement(const float delta)
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




FQuat FAngularKinematicCondition::GetAngularSpeedQuat(float time) const
{
	const FVector axis = RotationSpeed.GetSafeNormal();
	const float angle = FMath::DegreesToRadians(FMath::Clamp(RotationSpeed.Length() * time, 0, 360));
	const float halfTetha = angle * 0.5;
	const float sine = FMath::Sin(halfTetha);
	const float cosine = FMath::Cos(halfTetha);
	FQuat q = FQuat(axis.X * sine, axis.Y * sine, axis.Z * sine, cosine);
	return q;
}

FAngularKinematicCondition FAngularKinematicCondition::GetFinalCondition(double deltaTime) const
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




FKinematicComponents::FKinematicComponents()
{
}

FKinematicComponents::FKinematicComponents(FLinearKinematicCondition linearCond, FAngularKinematicCondition angularCond)
{
	LinearKinematic = linearCond;
	AngularKinematic = angularCond;
}

FKinematicComponents FKinematicComponents::FromComponent(FKinematicComponents fromComponent, double withDelta)
{
	LinearKinematic = fromComponent.LinearKinematic.GetFinalCondition(withDelta);
	AngularKinematic = fromComponent.AngularKinematic.GetFinalCondition(withDelta);
	return FKinematicComponents(LinearKinematic, AngularKinematic);
}

FKinematicComponents FKinematicComponents::FromComponent(FKinematicComponents fromComponent, FVector linearAcceleration,
	double withDelta)
{
	fromComponent.LinearKinematic.Acceleration = linearAcceleration;
	LinearKinematic = fromComponent.LinearKinematic.GetFinalCondition(withDelta);
	AngularKinematic = fromComponent.AngularKinematic.GetFinalCondition(withDelta);
	return FKinematicComponents(LinearKinematic, AngularKinematic);
}

FQuat FKinematicComponents::GetRotation() const
{
	return AngularKinematic.Orientation;
}




void FControllerStatus::ComputeDiffManifest(FControllerStatus diffDatas)
{
	TArray<bool> changedMap;
	if (FVector_NetQuantize(Kinematics.LinearKinematic.Velocity) != FVector_NetQuantize(diffDatas.Kinematics.LinearKinematic.Velocity)) changedMap.Add(true); else changedMap.Add(false); //[0]
	if (FVector_NetQuantize(Kinematics.LinearKinematic.Position) != FVector_NetQuantize(diffDatas.Kinematics.LinearKinematic.Position)) changedMap.Add(true); else changedMap.Add(false); //[1]
	FVector selfOrientationVector = FVector(0);
	FVector diffOrientationVector = FVector(0);
	float selfAngle = 0;
	float diffAngle = 0;
	Kinematics.AngularKinematic.Orientation.ToAxisAndAngle(selfOrientationVector, selfAngle);
	diffDatas.Kinematics.AngularKinematic.Orientation.ToAxisAndAngle(diffOrientationVector, diffAngle);
	if ((selfOrientationVector | diffOrientationVector) <= 0.8 || FMath::Abs(selfAngle - diffAngle) > FMath::DegreesToRadians(5)) changedMap.Add(true); else changedMap.Add(false); //[2]
	if (StatusParams.StateIndex != diffDatas.StatusParams.StateIndex) changedMap.Add(true); else changedMap.Add(false); //[3]
	if (StatusParams.ActionIndex != diffDatas.StatusParams.ActionIndex) changedMap.Add(true); else changedMap.Add(false); //[4]
	if (StatusParams.PrimaryStateFlag != diffDatas.StatusParams.PrimaryStateFlag) changedMap.Add(true); else changedMap.Add(false); //[5]
	if (StatusParams.PrimaryActionFlag != diffDatas.StatusParams.PrimaryActionFlag) changedMap.Add(true); else changedMap.Add(false); //[6]
	if (StatusParams.StateModifiers1 != diffDatas.StatusParams.StateModifiers1) changedMap.Add(true); else changedMap.Add(false); //[7]
	if (StatusParams.StateModifiers2 != diffDatas.StatusParams.StateModifiers2) changedMap.Add(true); else changedMap.Add(false); //[8]
	if (StatusParams.ActionsModifiers1 != diffDatas.StatusParams.ActionsModifiers1) changedMap.Add(true); else changedMap.Add(false); //[9]
	if (StatusParams.ActionsModifiers2 != diffDatas.StatusParams.ActionsModifiers2) changedMap.Add(true); else changedMap.Add(false); //[10]
	if (MoveInput != diffDatas.MoveInput) changedMap.Add(true); else changedMap.Add(false); //[11]
	if (CustomPhysicProperties != diffDatas.CustomPhysicProperties) changedMap.Add(true); else changedMap.Add(false); //[12]

	int manifest = UToolsLibrary::BoolArrayToInt(changedMap);
	DiffManifest = manifest;
}

void FControllerStatus::FromStatusDiff(int diffManifest, FControllerStatus diffDatas)
{
	TArray<bool> changedMap = UToolsLibrary::IntToBoolArray(diffManifest);
	if (changedMap.IsValidIndex(0) && changedMap[0]) Kinematics.LinearKinematic.Velocity = diffDatas.Kinematics.LinearKinematic.Velocity;
	if (changedMap.IsValidIndex(1) && changedMap[1]) Kinematics.LinearKinematic.Velocity = diffDatas.Kinematics.LinearKinematic.Position;
	if (changedMap.IsValidIndex(2) && changedMap[2]) Kinematics.AngularKinematic.Orientation = diffDatas.Kinematics.AngularKinematic.Orientation;
	if (changedMap.IsValidIndex(3) && changedMap[3]) StatusParams.StateIndex = diffDatas.StatusParams.StateIndex;
	if (changedMap.IsValidIndex(4) && changedMap[4]) StatusParams.ActionIndex = diffDatas.StatusParams.ActionIndex;
	if (changedMap.IsValidIndex(5) && changedMap[5]) StatusParams.PrimaryStateFlag = diffDatas.StatusParams.PrimaryStateFlag;
	if (changedMap.IsValidIndex(6) && changedMap[6]) StatusParams.PrimaryActionFlag = diffDatas.StatusParams.PrimaryActionFlag;
	if (changedMap.IsValidIndex(7) && changedMap[7]) StatusParams.StateModifiers1 = diffDatas.StatusParams.StateModifiers1;
	if (changedMap.IsValidIndex(8) && changedMap[8]) StatusParams.StateModifiers2 = diffDatas.StatusParams.StateModifiers2;
	if (changedMap.IsValidIndex(9) && changedMap[9]) StatusParams.ActionsModifiers1 = diffDatas.StatusParams.ActionsModifiers1;
	if (changedMap.IsValidIndex(10) && changedMap[10]) StatusParams.ActionsModifiers2 = diffDatas.StatusParams.ActionsModifiers2;
	if (changedMap.IsValidIndex(11) && changedMap[11]) MoveInput = diffDatas.MoveInput;
	if (changedMap.IsValidIndex(12) && changedMap[12]) CustomPhysicProperties = diffDatas.CustomPhysicProperties;
}

FControllerStatus FControllerStatus::GetDiffControllerStatus() const
{
	TArray<bool> changedMap = UToolsLibrary::IntToBoolArray(DiffManifest);
	FControllerStatus diff = FControllerStatus();
	diff.CustomPhysicProperties = FVector(0);
	if (changedMap.IsValidIndex(0) && changedMap[0]) diff.Kinematics.LinearKinematic.Velocity = Kinematics.LinearKinematic.Velocity;
	if (changedMap.IsValidIndex(1) && changedMap[1]) diff.Kinematics.LinearKinematic.Velocity = Kinematics.LinearKinematic.Position;
	if (changedMap.IsValidIndex(2) && changedMap[2]) diff.Kinematics.AngularKinematic.Orientation = Kinematics.AngularKinematic.Orientation;
	if (changedMap.IsValidIndex(3) && changedMap[3]) diff.StatusParams.StateIndex = StatusParams.StateIndex;
	if (changedMap.IsValidIndex(4) && changedMap[4]) diff.StatusParams.ActionIndex = StatusParams.ActionIndex;
	if (changedMap.IsValidIndex(5) && changedMap[5]) diff.StatusParams.PrimaryStateFlag = StatusParams.PrimaryStateFlag;
	if (changedMap.IsValidIndex(6) && changedMap[6]) diff.StatusParams.PrimaryActionFlag = StatusParams.PrimaryActionFlag;
	if (changedMap.IsValidIndex(7) && changedMap[7]) diff.StatusParams.StateModifiers1 = StatusParams.StateModifiers1;
	if (changedMap.IsValidIndex(8) && changedMap[8]) diff.StatusParams.StateModifiers2 = StatusParams.StateModifiers2;
	if (changedMap.IsValidIndex(9) && changedMap[9]) diff.StatusParams.ActionsModifiers1 = StatusParams.ActionsModifiers1;
	if (changedMap.IsValidIndex(10) && changedMap[10]) diff.StatusParams.ActionsModifiers2 = StatusParams.ActionsModifiers2;
	if (changedMap.IsValidIndex(11) && changedMap[11]) diff.MoveInput = MoveInput;
	if (changedMap.IsValidIndex(12) && changedMap[12]) diff.CustomPhysicProperties = CustomPhysicProperties;

	return diff;
}


#pragma endregion


#pragma region Network and Replication


void FNetKinematic::ExtractFromStatus(FControllerStatus status)
{
	MoveInput = status.MoveInput;
	Velocity = status.Kinematics.LinearKinematic.Velocity;
	Position = status.Kinematics.LinearKinematic.Position;
	Orientation = status.Kinematics.AngularKinematic.Orientation.Vector();
}

void FNetKinematic::RestoreOnToStatus(FControllerStatus& status) const
{
	status.MoveInput = MoveInput;
	status.Kinematics.LinearKinematic.Velocity = Velocity;
	status.Kinematics.LinearKinematic.Position = Position;
	status.Kinematics.AngularKinematic.Orientation = Orientation.ToOrientationQuat();
}



void FNetStatusParam::ExtractFromStatus(FControllerStatus status)
{
	StateIndex = status.StatusParams.StateIndex;
	ActionIndex = status.StatusParams.ActionIndex;
	StateFlag = status.StatusParams.PrimaryStateFlag;
	ActionFlag = status.StatusParams.PrimaryActionFlag;
}

void FNetStatusParam::RestoreOnToStatus(FControllerStatus& status) const
{
	status.StatusParams.StateIndex = StateIndex;
	status.StatusParams.ActionIndex = ActionIndex;
	status.StatusParams.PrimaryStateFlag = StateFlag;
	status.StatusParams.PrimaryActionFlag = ActionFlag;
}


#pragma endregion
