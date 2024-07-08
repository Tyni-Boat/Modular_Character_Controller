// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "CommonTypes.h"

#include "ToolsLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/NetSerialization.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


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

//------------------------------------------------------------------------------------------------------------------------

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
			_inputPool_last[entry.Key].HeldDuration = input.Phase == InputEntryPhase_Held
				                                          ? delta + _inputPool_last[entry.Key].HeldDuration
				                                          : 0;
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
			UKismetSystemLibrary::PrintString(worldContext, FString::Printf(
				                                  TEXT("Input: (%s), Nature: (%s), Phase: (%s), buffer: %f, Held: %f"),
				                                  *entry.Key.ToString(),
				                                  *UEnum::GetValueAsName<
					                                  EInputEntryNature>(_inputPool_last[entry.Key].Nature).ToString(),
				                                  *UEnum::GetValueAsName<EInputEntryPhase>(
					                                  _inputPool_last[entry.Key].Phase).ToString(), bufferChrono
				                                  , activeDuration), true, true, debugColor, 0, entry.Key);
		}
	}

	_inputPool.Empty();
}


#pragma endregion


#pragma region Surface and Zones

FHitResultExpanded::FHitResultExpanded()
{
}

FHitResultExpanded::FHitResultExpanded(FHitResult hit, ECollisionResponse queryType)
{
	HitResult = hit;
	QueryResponse = queryType != ECR_MAX ? queryType : (hit.bBlockingHit ? ECR_Block : ECR_Ignore);
}

//------------------------------------------------------------------------------------------------------------------------

FSurface::FSurface()
{
}

FSurface::FSurface(FHitResultExpanded hit, bool canStepOn)
{
	TrackedComponent = hit.HitResult.Component;
	UpdateHit(hit, canStepOn);
}

bool FSurface::UpdateTracking(float deltaTime)
{
	FVector linearVelocity = FVector(0);
	FVector angularVelocity = FVector(0);
	bool validSurface = false;

	if (TrackedComponent.IsValid())
	{
		validSurface = true;

		//Linear Part
		linearVelocity = _lastPosition.ContainsNaN() ? FVector(0) : (TrackedComponent->GetSocketLocation(TrackedComponentBoneName) - _lastPosition) / deltaTime;
		_lastPosition = TrackedComponent->GetSocketLocation(TrackedComponentBoneName);

		//Angular
		const FQuat targetQuat = TrackedComponent->GetSocketQuaternion(TrackedComponentBoneName);
		FQuat currentQuat = _lastRotation;

		//Get Angular speed
		if (!_lastRotation.ContainsNaN())
		{
			currentQuat.EnforceShortestArcWith(targetQuat);
			const FQuat quatDiff = targetQuat * currentQuat.Inverse();
			FVector axis;
			float angle;
			quatDiff.ToAxisAndAngle(axis, angle);
			axis.Normalize();
			angularVelocity = axis * FMath::RadiansToDegrees(angle / deltaTime);
		}
		_lastRotation = TrackedComponent->GetSocketQuaternion(TrackedComponentBoneName);
	}

	LinearVelocity = linearVelocity;
	AngularVelocity = angularVelocity;
	return validSurface;
}


void FSurface::UpdateHit(FHitResultExpanded hit, bool canStepOn)
{
	SurfacePoint = hit.HitResult.ImpactPoint;
	SurfaceNormal = hit.HitResult.Normal;
	SurfaceImpactNormal = hit.HitResult.ImpactNormal;
	if (TrackedComponentBoneName != hit.HitResult.BoneName)
	{
		_lastPosition = FVector(NAN);
		_lastRotation = FQuat(NAN,NAN,NAN,NAN);
		LinearVelocity = FVector(0);
		AngularVelocity = FVector(0);
	}
	TrackedComponentBoneName = hit.HitResult.BoneName;
	SurfacePhysicProperties = hit.HitResult.PhysMaterial.IsValid()
		                          ? FVector4f(hit.HitResult.PhysMaterial->Friction, hit.HitResult.PhysMaterial->Restitution, hit.QueryResponse, canStepOn? 1 : 0)
		                          : FVector4f(1, 0, hit.QueryResponse, canStepOn? 1 : 0);
}


FVector FSurface::ApplyForceAtOnSurface(const FVector point, const FVector force, bool reactionForce) const
{
	if (!TrackedComponent.IsValid())
		return FVector(0);
	if (!TrackedComponent->IsSimulatingPhysics(TrackedComponentBoneName))
		return FVector(0);
	FVector f = force;
	if (reactionForce)
	{
		f = (f | SurfaceNormal) >= 0 ? FVector(0) : f.ProjectOnToNormal(SurfaceNormal);
	}
	const FVector lastVelocityAt = TrackedComponent->GetPhysicsLinearVelocityAtPoint(point, TrackedComponentBoneName);
	TrackedComponent->AddForceAtLocation(f, point, TrackedComponentBoneName);
	return lastVelocityAt;
}

FVector FSurface::GetVelocityAlongNormal(const FVector velocity, const bool useImpactNormal, const bool reactionPlanarOnly) const
{
	if (!TrackedComponent.IsValid())
		return velocity;

	const FVector normal = useImpactNormal ? SurfaceImpactNormal : SurfaceNormal;
	if (reactionPlanarOnly && (normal | velocity) > 0)
		return velocity;
	return FVector::VectorPlaneProject(velocity, normal);
}


FVector FSurface::GetVelocityAt(const FVector point, const float deltaTime) const
{
	FVector linearPart = LinearVelocity;
	if (SurfaceNormal.SquaredLength() > 0 && linearPart.SquaredLength() > 0)
	{
		linearPart = (linearPart.GetSafeNormal() | SurfaceNormal) >= 0 ? LinearVelocity : FVector::VectorPlaneProject(LinearVelocity, SurfaceNormal);
	}
	if (!TrackedComponent.IsValid())
		return linearPart;

	//Angular part
	const FVector rotationAxis = AngularVelocity.GetSafeNormal();
	const FVector radiusDirection = FVector::VectorPlaneProject(point - TrackedComponent->GetSocketLocation(TrackedComponentBoneName), rotationAxis).GetSafeNormal();
	FVector tangentialDirection = FVector::CrossProduct(rotationAxis, radiusDirection);
	tangentialDirection.Normalize();
	const double radius = FVector::VectorPlaneProject(point - TrackedComponent->GetSocketLocation(TrackedComponentBoneName), rotationAxis).Length();
	const double angle = FMath::DegreesToRadians(AngularVelocity.Length());
	const FVector rotVel = radius * angle * tangentialDirection;
	const FVector centripetal = -radiusDirection * (angle * angle) * radius * deltaTime * deltaTime * 1.5;//(1 + deltaTime);

	//Finally
	return linearPart + rotVel + centripetal;
}


#pragma endregion


#pragma region States and Actions


FActionInfos::FActionInfos()
{
}

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
				return 1 - ((_remainingActivationTimer - (_startingDurations.Y + _startingDurations.Z)) / _startingDurations
					.X);
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
		default:
			break;
	}
}

void FActionInfos::Update(float deltaTime, bool allowCooldownDecrease)
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
		else if (_remainingActivationTimer > _startingDurations.Z && _remainingActivationTimer <= (_startingDurations.Y
			+ _startingDurations.Z))
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
		if (_cooldownTimer > 0 && allowCooldownDecrease)
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


//------------------------------------------------------------------------------------------------------------------------


FActionMotionMontage::FActionMotionMontage()
{
}


//------------------------------------------------------------------------------------------------------------------------


FStatusParameters::FStatusParameters()
{
}

bool FStatusParameters::HasChanged(FStatusParameters otherStatus) const
{
	const bool stateChange = StateIndex != otherStatus.StateIndex;
	const bool stateFlagChange = PrimaryStateFlag != otherStatus.PrimaryStateFlag;
	const bool actionChange = ActionIndex != otherStatus.ActionIndex;
	const bool actionFlagChange = PrimaryActionFlag != otherStatus.PrimaryActionFlag;
	return stateChange || stateFlagChange || actionChange || actionFlagChange;
}

#pragma endregion


#pragma region Movement


FLinearKinematicCondition::FLinearKinematicCondition()
{
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
		const double acceleration = moveParam.W >= 0 ? moveParam.W : FMath::Abs(moveParam.W) * (1 / (delta * delta));
		if (acceleration <= 0)
			continue;
		const double t = FMath::Clamp(acceleration * delta, 0, 1 / delta);
		const FVector v = movement;
		const FVector v0 = relativeVelocity;
		FVector a = FVector(0);
		a.X = (v.X - v0.X) * t;
		a.Y = (v.Y - v0.Y) * t;
		a.Z = (v.Z - v0.Z) * t;
		Acceleration += a;
	}
}


//------------------------------------------------------------------------------------------------------------------------


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


//------------------------------------------------------------------------------------------------------------------------


FKinematicComponents::FKinematicComponents()
{
}

FKinematicComponents::FKinematicComponents(FLinearKinematicCondition linearCond, FAngularKinematicCondition angularCond, TArray<FSurface>* surfaces, int surfacesActive)
{
	LinearKinematic = linearCond;
	AngularKinematic = angularCond;
	if (surfaces)
		SurfacesInContact = *surfaces;
	SurfaceBinaryFlag = surfacesActive;
}

bool FKinematicComponents::ForEachSurface(std::function<void(FSurface)> doAction, bool onlyValidOnes) const
{
	if (SurfacesInContact.Num() <= 0)
		return false;

	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(SurfaceBinaryFlag);
	if (onlyValidOnes)
	{
		if (surfaceCombination.Num() <= 0)
			return false;
	}

	for (int i = 0; i < SurfacesInContact.Num(); i++)
	{
		if(onlyValidOnes)
		{
			if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
				continue;
		}
		const auto surface = SurfacesInContact[i];
		doAction(surface);
	}

	return true;
}

FQuat FKinematicComponents::GetRotation() const
{
	return AngularKinematic.Orientation;
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


//------------------------------------------------------------------------------------------------------------------------


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
