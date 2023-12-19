// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "StateBehaviours/GroundState.h"
#include <Kismet/KismetSystemLibrary.h>
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Pawn.h"


#pragma region Check

bool UGroundState::CheckSurface(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	_snapVector = FVector(0);
	GroundState = EGroundStateMode::GroundStateMode_No_Ground;

	if (inDatas.GetActor() == nullptr)
		return false;

	//are we in cool down?
	if (_checkDelayChrono > 0)
	{
		_checkDelayChrono -= inDelta;
		return false;
	}

	//Caching values
	FVector direction = inDatas.Gravity.GetSafeNormal();
	if (!direction.Normalize())
		direction = -FVector::UpVector;
	FVector userMoveDir = inputs.ReadInput(MovementInputName).Axis;
	FVector startPt = inDatas.InitialTransform.GetLocation() + CheckOffset;
	FVector controllerVelocity = inDatas.GetInitialMomentum();
	float rayCastingDistance = MaxCheckDistance + SnapToSurfaceDistance;

	//Reset values
	_userMoveDirection = userMoveDir;


	//Tracing
	FHitResult centralHit;
	FHitResult directionalHit;
	FHitResult areaHit;
	TArray<AActor*> ignoredActors;
	ignoredActors.Add(inDatas.GetActor());

	TArray<FHitResult> NormalHits;

	//Central hit check
	{
		FVector customStart = startPt;
		float hyp = CheckRadius / FMath::Sin(FMath::DegreesToRadians(MaxSurfaceAngle));
		float angledDistance = hyp * FMath::Cos(FMath::DegreesToRadians(MaxSurfaceAngle));
		if (inDatas.IsDebugMode) 
		{
			UKismetSystemLibrary::DrawDebugArrow(inDatas.GetActor(), customStart, customStart + direction * (rayCastingDistance + angledDistance), 50, FColor::White, 0, 0.5f);
		}

		UKismetSystemLibrary::LineTraceSingle(inDatas.GetActor(), customStart, customStart + direction * (rayCastingDistance + angledDistance), ChannelGround, CanTraceComplex, ignoredActors, inDatas.IsDebugMode ? EDrawDebugTrace::None : EDrawDebugTrace::None, centralHit, true, FColor::White, FColor::White, -1);
	}

	//Directionnal hit check
	{
		FVector customStart = startPt + direction * CheckRadius;
		FVector radiusOffset = FVector::VectorPlaneProject(userMoveDir, direction).GetSafeNormal() * MaxStepUpDistance;
		if (radiusOffset.Length() > 0)
		{
			UKismetSystemLibrary::SphereTraceSingle(inDatas.GetActor(), customStart + radiusOffset, customStart + radiusOffset + direction * (rayCastingDistance - 2 * CheckRadius), CheckRadius, ChannelGround, CanTraceComplex, ignoredActors, inDatas.IsDebugMode ? EDrawDebugTrace::None : EDrawDebugTrace::None, directionalHit, true, FColor::Transparent, FColor::White, -1);
		}
	}

	//Area check
	{
		switch (CheckShape)
		{
		case ShapeMode_Cube:
		{
			UKismetSystemLibrary::BoxTraceSingle(inDatas.GetActor(), startPt, startPt + direction * rayCastingDistance, FVector::One() * CheckRadius, inDatas.InitialTransform.Rotator(), ChannelGround, CanTraceComplex, ignoredActors, inDatas.IsDebugMode ? EDrawDebugTrace::None : EDrawDebugTrace::None, areaHit, true, FColor::Transparent, FColor::Green, -1);
		}break;
		default:
		{
			FVector customStart = startPt + direction * CheckRadius;
			UKismetSystemLibrary::SphereTraceSingle(inDatas.GetActor(), customStart, customStart + direction * (rayCastingDistance - 2 * CheckRadius), CheckRadius, ChannelGround, CanTraceComplex, ignoredActors, inDatas.IsDebugMode ? EDrawDebugTrace::None : EDrawDebugTrace::None, areaHit, true, FColor::Transparent, FColor::Green, -1);
		}break;
		}
	}

	//Include additionnals traces in the pipe
	if (centralHit.Component != nullptr)
	{
		NormalHits.Add(centralHit);
	}
	if (areaHit.Component != nullptr)
	{
		NormalHits.Add(areaHit);
	}
	if (directionalHit.Component != nullptr)
	{
		directionalHit.Normal = directionalHit.ImpactNormal;
		NormalHits.Add(directionalHit);
	}

	if (NormalHits.Num() > 0)
	{
		FHitResult selectedSurface;
		FVector surfaceOffset;

		//Select the best surface
		if (true)
		{
			int surfaceIndex = -1;
			TArray<FHitResult> validSurfaces;

			//Separate step up contacts from normal ones
			{
				float comparisionWidth = MaxSurfaceOffsetRatio;
				if (MaxSurfaceOffsetRatio >= 0 && MaxSurfaceOffsetRatio < 1)
				{
					comparisionWidth = CheckRadius * MaxSurfaceOffsetRatio;
				}
				for (int i = 0; i < NormalHits.Num(); i++)
				{
					bool validated = true;
					float angle = FVector::DotProduct(NormalHits[i].Normal, -direction);
					float degAngle = FMath::RadiansToDegrees(FMath::Acos(angle));
					float offset = FVector::VectorPlaneProject(NormalHits[i].ImpactPoint - startPt, direction).Length();
					float dist = (NormalHits[i].ImpactPoint - startPt).ProjectOnToNormal(direction).Length();

					if (dist < (SnapToSurfaceDistance - MaxStepHeight) && offset >= CheckRadius)
						validated = false;

					if (degAngle >= MaxSurfaceAngle)
						validated = false;

					//if (offset >= comparisionWidth)
					//	validated = false;

					if (!validated && inDatas.IsDebugMode)
					{
						FVector up = NormalHits[i].Normal;
						FVector forward = FVector::DotProduct(NormalHits[i].Normal,
							inDatas.InitialTransform.GetRotation().GetUpVector()) < 1
							? (FVector::VectorPlaneProject(
								FVector::VectorPlaneProject(
									NormalHits[i].Normal,
									inDatas.InitialTransform.GetRotation().GetUpVector()).
								GetSafeNormal(), NormalHits[i].Normal).GetSafeNormal())
							: inDatas.InitialTransform.GetRotation().GetRightVector();
						FVector right = FVector::CrossProduct(up, forward);
						UKismetSystemLibrary::DrawDebugCircle(inDatas.GetActor(), NormalHits[i].ImpactPoint, CheckRadius, 32,
							FColor::White, 0, 1, right, forward);
						continue;
					}

					validSurfaces.Add(NormalHits[i]);
				}
			}

			//Fall back into normal ones validity
			{
				if (surfaceIndex < 0 && validSurfaces.Num() > 0)
				{
					surfaceIndex = -1;
					float surfaceAngle = FVector::DotProduct(validSurfaces[0].ImpactNormal, -direction);
					float surfaceDistance = std::numeric_limits<float>::max();
					float surface_Offset = std::numeric_limits<float>::max();
					TArray<FVector4> priorityArray;
					for (int i = 0; i < validSurfaces.Num(); i++)
					{
						float angle = FVector::DotProduct(validSurfaces[i].ImpactNormal, -direction);
						float dist = (validSurfaces[i].ImpactPoint - startPt).ProjectOnToNormal(direction).Length();
						float offset = FVector::VectorPlaneProject(validSurfaces[i].ImpactPoint - startPt, direction).
							Length();
						if (inDatas.IsDebugMode)
						{
							//UKismetSystemLibrary::DrawDebugArrow(inDatas.GetActor(), validSurfaces[i].ImpactPoint,
							//	validSurfaces[i].ImpactPoint + validSurfaces[i].ImpactNormal * 50,
							//	50, FColor::Yellow, 0, 2);

							//FVector up = validSurfaces[i].Normal;
							//FVector forward = FVector::DotProduct(validSurfaces[i].Normal,
							//	inDatas.InitialTransform.GetRotation().GetUpVector())
							//	< 1
							//	? (FVector::VectorPlaneProject(
							//		FVector::VectorPlaneProject(
							//			validSurfaces[i].Normal,
							//			inDatas.InitialTransform.GetRotation().GetUpVector()).
							//		GetSafeNormal(), validSurfaces[i].Normal).GetSafeNormal())
							//	: inDatas.InitialTransform.GetRotation().GetRightVector();
							//FVector right = FVector::CrossProduct(up, forward);
							//UKismetSystemLibrary::DrawDebugCircle(inDatas.GetActor(), validSurfaces[i].ImpactPoint, CheckRadius,
							//	16, FColor::Yellow, 0, 2, right, forward);
						}
						priorityArray.Add(FVector4(i, angle, offset, dist));
					}

					if (priorityArray.Num() > 0)
					{
						priorityArray.Sort([](const FVector4 a, const FVector4 b) {
							return a.W < b.W;
							});
						surfaceIndex = priorityArray[0].X;

						if (validSurfaces.IsValidIndex(surfaceIndex))
						{
							selectedSurface = validSurfaces[surfaceIndex];
						}
					}
				}
			}

			if (surfaceIndex < 0 || selectedSurface.GetComponent() == nullptr)
			{
				return false;
			}

			if (inDatas.IsDebugMode)
			{
				UKismetSystemLibrary::DrawDebugArrow(inDatas.GetActor(), selectedSurface.ImpactPoint,
					selectedSurface.ImpactPoint + selectedSurface.ImpactNormal * 10,
					50, FColor::Green, 0, 2);

				FVector up = selectedSurface.Normal;
				FVector forward = FVector::DotProduct(selectedSurface.Normal,
					inDatas.InitialTransform.GetRotation().GetUpVector()) < 1
					? (FVector::VectorPlaneProject(
						FVector::VectorPlaneProject(selectedSurface.Normal,
							inDatas.InitialTransform.GetRotation().
							GetUpVector()).GetSafeNormal(),
						selectedSurface.Normal).GetSafeNormal())
					: inDatas.InitialTransform.GetRotation().GetRightVector();
				FVector right = FVector::CrossProduct(up, forward);
				UKismetSystemLibrary::DrawDebugCircle(inDatas.GetActor(), selectedSurface.ImpactPoint, CheckRadius, 32,
					FColor::Green, 0, 2, right, forward);
			}

			surfaceOffset = FVector::VectorPlaneProject(selectedSurface.ImpactPoint - startPt, direction);
		}

		//Stair cases verification
		if (true)
		{
			if (selectedSurface.IsValidBlockingHit())
			{
				GroundState = EGroundStateMode::GroundStateMode_StableGround;
			}
			if (surfaceOffset.Length() > 0 && centralHit.Component != nullptr)
			{
				float degAngleNormals = FMath::RadiansToDegrees(
					FMath::Acos(FVector::DotProduct(selectedSurface.Normal, centralHit.ImpactNormal)));
				GroundState = degAngleNormals > 5 ? EGroundStateMode::GroundStateMode_StairCases : EGroundStateMode::GroundStateMode_StableGround;
			}

			StateFlag = GroundState;
		}

		//Surface movement tracking
		{
			SurfaceInfos.UpdateSurfaceInfos(inDatas.GetActor(), selectedSurface, inDelta);
		}

		//Snapping
		if (selectedSurface.IsValidBlockingHit())
		{
			FVector snapDir = SnapToGround(selectedSurface.ImpactPoint, inDatas, controller);

			if (snapDir.Length() >= 0 && FVector::DotProduct(snapDir, -direction) >= 0)
			{
				if (!_touchedGroundReal)
				{
					OnLanding(SurfaceInfos, inDatas, inDelta);
					_touchedGroundReal = true;
				}
				_snapVector = snapDir* SnapToSurfaceUpSpeed;
			}
			else if (_touchedGroundReal)
			{
				_snapVector = snapDir* SnapToSurfaceDownSpeed;
			}
		}

		return _touchedGroundReal;
	}

	return false;
}


void UGroundState::OnLanding_Implementation(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas, const float delta)
{
	//Landing Force
	if (inDatas.bUsePhysic && inDatas.GetInitialMomentum().Length() > 0 && landingSurface.GetHitResult().IsValidBlockingHit() && landingSurface.GetSurfacePrimitive() != nullptr && landingSurface.GetSurfacePrimitive()->IsSimulatingPhysics())
	{
		float dotProduct = -FVector::DotProduct(inDatas.GetInitialMomentum().GetSafeNormal(), landingSurface.GetSurfaceNormal().GetSafeNormal());
		landingSurface.GetSurfacePrimitive()->AddImpulseAtLocation((inDatas.GetInitialMomentum() / delta) * FMath::Clamp(dotProduct, 0, 1), landingSurface.GetHitResult().ImpactPoint, landingSurface.GetHitResult().BoneName);
	}
}


void UGroundState::OnTakeOff_Implementation(FSurfaceInfos landingSurface, const FKinematicInfos& inDatas)
{

}

#pragma endregion




#pragma region Movement

FVector UGroundState::MoveOnTheGround(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta)
{
	FVector horizontalVelocity = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
	FVector inputMove = _userMoveDirection;
	if (inputMove.SquaredLength() > 0)
	{
		float moveAmount = inputMove.Length();
		FVector onGravPlanedInputs = FMath::Lerp(horizontalVelocity, inputMove * MaxMoveSpeed, inDelta * Acceleration);

		float moveLenght = onGravPlanedInputs.Length();
		inputMove.Normalize();

		FVector scaledInputs = onGravPlanedInputs;
		CurrentSpeedRatio = FMath::Lerp(CurrentSpeedRatio, 1, inDelta * Acceleration);

		if (inDatas.bUsePhysic && inDatas.FinalSurface.GetHitResult().IsValidBlockingHit() && inDatas.FinalSurface.GetSurfacePrimitive() != nullptr && inDatas.FinalSurface.GetSurfacePrimitive()->IsSimulatingPhysics() && horizontalVelocity.Length() > 0)
		{
			inDatas.FinalSurface.GetSurfacePrimitive()->AddForceAtLocation(FVector::VectorPlaneProject(-horizontalVelocity, inDatas.FinalSurface.GetHitResult().Normal) * inDatas.GetMass(), inDatas.FinalSurface.GetHitResult().ImpactPoint, inDatas.FinalSurface.GetHitResult().BoneName);
		}

		return scaledInputs;
	}
	else
	{
		float decc = FMath::Clamp(Decceleration, 1, TNumericLimits<float>().Max());
		FVector scaledInputs = FMath::Lerp(horizontalVelocity, FVector::ZeroVector, inDelta * decc);
		CurrentSpeedRatio = FMath::Lerp(CurrentSpeedRatio, 0, inDelta * decc);

		return scaledInputs;
	}
}

#pragma endregion



#pragma region Snapping


FVector UGroundState::SnapToGround(const FVector hitPoint, const FKinematicInfos& inDatas, UModularControllerComponent* controller)
{
	FVector snapForce = FVector(0);
	FVector direction = inDatas.Gravity.GetSafeNormal();
	float reduction = 0;
	switch (CheckShape)
	{
	case ShapeMode_Cube:
		reduction = 0;
		break;
	default:
		reduction = CheckRadius;
		break;
	}
	FVector desiredPt = inDatas.InitialTransform.GetLocation() + CheckOffset + direction * (SnapToSurfaceDistance - reduction);
	if (controller)
	{
		FVector ptOnShape = controller->PointOnShape(direction, inDatas.InitialTransform.GetLocation());
		if ((ptOnShape - inDatas.InitialTransform.GetLocation()).Length() >= (desiredPt - inDatas.InitialTransform.GetLocation()).Length())
		{
			desiredPt = ptOnShape;
		}
	}

	FVector ptVector = (hitPoint - desiredPt);
	FVector snapVector = ptVector.ProjectOnToNormal(direction);
	snapForce = snapVector.GetSafeNormal() * snapVector.Length();

	if (inDatas.IsDebugMode && snapForce.SquaredLength() > 1)
	{
		UKismetSystemLibrary::DrawDebugArrow(inDatas.GetActor(), desiredPt, desiredPt + snapForce, 50, FColor::Black, 0, 3);
	}
	return snapForce;
}

#pragma endregion



#pragma region Slide

FVector UGroundState::SlideOnTheGround(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta)
{
	FVector horizontalVelocity = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), inDatas.Gravity.GetSafeNormal());
	FVector planedNormal = FVector::VectorPlaneProject(SurfaceInfos.GetSurfaceNormal(), -inDatas.Gravity.GetSafeNormal()).GetSafeNormal();
	if (planedNormal.SquaredLength() > 0)
	{
		float moveAmount = planedNormal.Length();
		FVector onGravPlanedInputs = FMath::Lerp(horizontalVelocity, planedNormal * SlidingSpeed, inDelta * SlidingAcceleration);

		float moveLenght = onGravPlanedInputs.Length();
		planedNormal.Normalize();

		FVector scaledInputs = onGravPlanedInputs;
		if (inDatas.IsDebugMode && inDatas.GetActor() != nullptr)
		{
			UKismetSystemLibrary::DrawDebugArrow(inDatas.GetActor(), inDatas.InitialTransform.GetLocation(), inDatas.InitialTransform.GetLocation() + scaledInputs, 50, FColor::White, 0, 10);
		}
		return scaledInputs;
	}
	else
	{
		FVector scaledInputs = FMath::Lerp(horizontalVelocity, FVector::ZeroVector, inDelta * Decceleration);

		return scaledInputs;
	}
}

#pragma endregion



#pragma region Functions

int UGroundState::GetPriority_Implementation()
{
	return BehaviourPriority;
}

FName UGroundState::GetDescriptionName_Implementation()
{
	return BehaviourName;
}

void UGroundState::StateIdle_Implementation(UModularControllerComponent* controller, const float inDelta)
{
}

bool UGroundState::CheckState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return CheckSurface(inDatas, inputs, controller, inDelta);
}

void UGroundState::OnEnterState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	FVector verticalVelocity = inDatas.InitialVelocities.ConstantLinearVelocity.ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());
	_landingVelocity = verticalVelocity;
}

FVelocity UGroundState::ProcessState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	FVelocity result = FVelocity();
	result.Rotation = inDatas.InitialTransform.GetRotation();
	FVector counterGravityDir = -inDatas.Gravity.GetSafeNormal();
	FVector up = counterGravityDir;

	FVector lookDirection = inDatas.InitialTransform.GetRotation().Vector();
	bool rotate = false;


	//Rotate
	if (_userMoveDirection.Length() > 0)
	{
		FVector inputAxis = _userMoveDirection;

		inputAxis.Normalize();
		FVector fwd = FVector::VectorPlaneProject(inputAxis, up);
		fwd.Normalize();
		FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
		FQuat rotation = FQuat::Slerp(inDatas.InitialTransform.GetRotation(), fwdRot, FMath::Clamp(inDelta * TurnSpeed, 0, 1));
		result.Rotation = rotation;
	}

	if (_touchedGroundReal)
	{
		float angle = FVector::DotProduct(SurfaceInfos.GetSurfaceNormal(), counterGravityDir);
		float degAngle = FMath::RadiansToDegrees(FMath::Acos(angle));
		FVector verticalVelocity = inDatas.GetInitialMomentum().ProjectOnToNormal(inDatas.Gravity.GetSafeNormal());

		if (degAngle >= MaxSurfaceAngle * SurfaceGripRatio && GroundState != GroundStateMode_StairCases)
		{
			//SLiding
			result._rooMotionScale = 0;
			GroundState = EGroundStateMode::GroundStateMode_SlidingSurface;
			FVector moveVec = SlideOnTheGround(inDatas, inputs, inDelta);
			result.ConstantLinearVelocity = moveVec;
			CurrentSpeedRatio = 0;
		}
		else
		{
			if (degAngle >= NormalConeAngle)
			{
				FVector normalizedPlanedMove = FVector::VectorPlaneProject(_userMoveDirection, counterGravityDir).GetSafeNormal();
				float moveToHill = FVector::DotProduct(normalizedPlanedMove, SurfaceInfos.GetSurfaceNormal());
				result._rooMotionScale = moveToHill <= 0 ? FVector::VectorPlaneProject(normalizedPlanedMove, SurfaceInfos.GetSurfaceNormal()).Length() : 1;
			}

			//Walking
			FVector moveVec = MoveOnTheGround(inDatas, inputs, inDelta);
			result.ConstantLinearVelocity = moveVec;
			result.ConstantLinearVelocity *= result._rooMotionScale;
		}

		//Surface velocity
		FVector snapVertical = _snapVector.ProjectOnToNormal(up);

		//Snap
		float snapSpeed = 0;
		if (snapVertical.Length() >= 0 && FVector::DotProduct(snapVertical, up) >= 0)
			snapSpeed = SnapToSurfaceUpSpeed;
		else
			snapSpeed = SnapToSurfaceDownSpeed;

		//Instant velocities
		FQuat instantRot = SurfaceInfos.GetSurfaceAngularVelocity();
		result.Rotation *= instantRot;

		APawn* pawn = nullptr;
		if (inDatas.GetActor())
		{
			pawn = Cast<APawn>(inDatas.GetActor());
		}
		if (pawn != nullptr && pawn->IsLocallyControlled())
		{
			result.InstantLinearVelocity = _snapVector* (GroundState == GroundStateMode_SlidingSurface ? 1 / snapSpeed : 1) + SurfaceInfos.GetSurfaceLinearVelocity();
		}
		else
		{
			result.InstantLinearVelocity = FVector(0);
		}
	}
	else
	{
		result.ConstantLinearVelocity = inDatas.GetInitialMomentum() + inDatas.Gravity * inDelta;
	}

	return result;
}

void UGroundState::OnExitState_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	OnTakeOff(inDatas.FinalSurface, inDatas);
	_checkDelayChrono = CheckDelay;
	_touchedGroundReal = false;
	CurrentSpeedRatio = 0;
}

void UGroundState::OnBehaviourChanged_Implementation(FName newBehaviourDescName, int newPriority, UModularControllerComponent* controller)
{
}

FString UGroundState::DebugString()
{
	return Super::DebugString() + " : " + UEnum::GetValueAsName<EGroundStateMode>(GroundState).ToString() + " ; "
		+ (SurfaceInfos.GetSurfacePrimitive() && SurfaceInfos.GetSurfacePrimitive()->GetOwner() ? SurfaceInfos.GetSurfacePrimitive()->GetOwner()->GetName() : "");
}


void UGroundState::ComputeFromFlag_Implementation(int flag)
{
	Super::ComputeFromFlag_Implementation(flag);
	GroundState = static_cast<EGroundStateMode>(flag);
}

#pragma endregion


