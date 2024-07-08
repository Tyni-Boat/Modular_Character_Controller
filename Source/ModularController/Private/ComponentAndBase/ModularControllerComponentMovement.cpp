// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "CollisionQueryParams.h"
#include "ToolsLibrary.h"


#pragma region Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void UModularControllerComponent::Move_Implementation(const FKinematicComponents finalKinematic, float deltaTime)
{
	if (!UpdatedPrimitive)
		return;

	const FVector posOffset = GetLocation() - _lastLocation;
	const FQuat rotOffset = GetRotation() * _lastRotation.Inverse();

	if (UpdatedPrimitive->IsSimulatingPhysics())
	{
		UpdatedPrimitive->SetAllPhysicsPosition(finalKinematic.LinearKinematic.Position);
		UpdatedPrimitive->SetAllPhysicsLinearVelocity(finalKinematic.LinearKinematic.Velocity);
		UpdatedPrimitive->SetAllPhysicsRotation(finalKinematic.AngularKinematic.Orientation);
	}
	else
	{
		const FQuat orientation = finalKinematic.AngularKinematic.Orientation;
		UpdatedPrimitive->SetWorldRotation(orientation * rotOffset, false);
		UpdatedPrimitive->SetWorldLocation(finalKinematic.LinearKinematic.Position + posOffset, false);
	}
}


FKinematicComponents UModularControllerComponent::KinematicMoveEvaluation(FControllerStatus processedMove, bool noCollision, float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("KinematicMoveEvaluation");
	FKinematicComponents finalKcomp = processedMove.Kinematics;
	const FQuat primaryRotation = processedMove.Kinematics.GetRotation();
	FVector initialLocation = processedMove.Kinematics.LinearKinematic.Position;
	const FVector surfacesProps = UFunctionLibrary::GetMaxSurfacePhysicProperties(finalKcomp);

	constexpr float hull = -0.1;
	const double drag = processedMove.CustomPhysicDrag >= 0 ? processedMove.CustomPhysicDrag : Drag;

	//Drag
	{
		FVector vel = UFunctionLibrary::GetRelativeVelocity(finalKcomp, delta) * 0.01;
		const double velSqr = vel.SquaredLength();
		if (vel.Normalize())
		{
			finalKcomp.LinearKinematic.Acceleration -= vel * ((velSqr * drag) / (2 * GetMass())) * 100;
		}
	}

	const FVector externalAcceleration = _externalForces.ContainsNaN() || _externalForces.IsNearlyZero() ? FVector(0) : _externalForces / GetMass();
	//Surface and external forces movement
	{
		FVector surfaceVelocity = UFunctionLibrary::GetAverageSurfaceVelocityAt(finalKcomp, initialLocation, delta);
		const FVector externalVelocity = externalAcceleration * delta;
		if (UFunctionLibrary::IsValidSurfaces(finalKcomp))
		{
			UFunctionLibrary::SetReferentialMovement(finalKcomp.LinearKinematic,
			                                         surfaceVelocity + (externalVelocity / FMath::Clamp(surfacesProps.X, TNumericLimits<float>::Min(), TNumericLimits<float>::Max())),
			                                         delta, surfacesProps.X * (1 / (delta * delta)));
		}
		else
		{
			finalKcomp.LinearKinematic.refAcceleration = externalAcceleration;
			finalKcomp.LinearKinematic.refVelocity = externalVelocity;
		}
	}

	//Snap displacement
	FVector snapMove = finalKcomp.LinearKinematic.SnapDisplacement;
	if ((externalAcceleration | snapMove) < 0)
		snapMove = FVector(0);
	if (snapMove.SquaredLength() > 0)
	{
		FHitResult sweepSnap;
		FCollisionQueryParams queryParams;
		queryParams.AddIgnoredComponents(IgnoredCollisionComponents);
		const bool snapHit = ComponentTraceCastSingle_Internal(sweepSnap, initialLocation, snapMove, primaryRotation, -1, bUseComplexCollision, queryParams);
		if (snapHit)
		{
			int deep = 1;
			//const FVector ptSweep = SlideAlongSurfaceAt(sweepSnap, primaryRotation, snapMove, deep, delta, hull);
			initialLocation = sweepSnap.Location - snapMove.GetSafeNormal() * 1;
		}
		else initialLocation += snapMove;
		finalKcomp.LinearKinematic.Position = initialLocation;
	}


	//Kinematic function to evaluate position and velocity from acceleration
	finalKcomp.LinearKinematic = finalKcomp.LinearKinematic.GetFinalCondition(delta);

	//Movement Sweep Test
	{
		FVector displacement = finalKcomp.LinearKinematic.Position - initialLocation;

		FHitResult sweepMoveHit = FHitResult(EForceInit::ForceInitToZero);
		FCollisionQueryParams queryParams;
		queryParams.AddIgnoredComponents(IgnoredCollisionComponents);
		bool blockingHit = false;
		FVector endLocation = initialLocation;
		FVector dDir = displacement.GetSafeNormal();

		//Trace to detect hit while moving
		blockingHit = noCollision
			              ? false
			              : ComponentTraceCastSingle_Internal(sweepMoveHit, initialLocation - dDir * FMath::Abs(hull), displacement + (dDir * FMath::Abs(hull)), primaryRotation, hull,
			                                                  bUseComplexCollision, queryParams);

		// Handle collision
		if (blockingHit) // && (sweepMoveHit.Normal | displacement) <= 0)
		{
			const FVector hitNormal = sweepMoveHit.Normal;
			int maxDepth = 1;
			endLocation = SlideAlongSurfaceAt(sweepMoveHit, primaryRotation, displacement, maxDepth, delta, hull);

			//Stuck protection
			if (sweepMoveHit.bStartPenetrating && UpdatedPrimitive && initialLocation.Equals(endLocation, 0.35) && displacement.SquaredLength() > 0)
			{
				if (DebugType == ControllerDebugType_MovementDebug)
				{
					UKismetSystemLibrary::PrintString(
						this, FString::Printf(TEXT("I'm stuck: initial location: (%s). End location: (%s)"), *initialLocation.ToCompactString(), *endLocation.ToCompactString())
						, true, false, FColor::Magenta, delta * 2, "stuck");
					UKismetSystemLibrary::DrawDebugArrow(this, sweepMoveHit.ImpactPoint, sweepMoveHit.ImpactPoint + hitNormal * 50
					                                     , 50, FColor::Magenta, delta * 2, 3);
				}

				FVector depenetrationVector = FVector(0);
				const auto depShape = UpdatedPrimitive->GetCollisionShape(0.125);
				ECollisionChannel shapeResponse = UpdatedPrimitive->GetCollisionObjectType();
				finalKcomp.ForEachSurface([&depenetrationVector, sweepMoveHit, depShape, endLocation, primaryRotation, shapeResponse](FSurface surface) -> void
				{
					const ECollisionResponse collisionResponse = surface.TrackedComponent->GetCollisionResponseToChannel(shapeResponse);
					const bool isBlocking = collisionResponse == ECollisionResponse::ECR_Block;
					if (!isBlocking)
						return;
					FMTDResult penetrationInfos;
					if (surface.TrackedComponent.IsValid() && surface.TrackedComponent->ComputePenetration(penetrationInfos, depShape, endLocation, primaryRotation))
					{
						const FVector depForce = penetrationInfos.Direction * penetrationInfos.Distance;
						depenetrationVector += depForce;
					}
				}, false);
				depenetrationVector += hitNormal * 0.125;
				endLocation += depenetrationVector;
				finalKcomp.LinearKinematic.Velocity = FVector(0);
			}

			//Debug Movement
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Move Amount Done: (%f) percent. Initial overlap? (%d)"), sweepMoveHit.Time * 100
				                                                        , sweepMoveHit.bStartPenetrating), true, false, FColor::Red, delta, "hitTime");
			}

			//Handle new positioning and Velocity
			finalKcomp.LinearKinematic.Position = endLocation - dDir * FMath::Abs(hull);
			finalKcomp.LastMoveHit = sweepMoveHit;
			finalKcomp.LastMoveHit.HitResult.Location = endLocation;
			finalKcomp.LastMoveHit.HitResult.TraceStart = initialLocation;
			finalKcomp.LastMoveHit.CustomTraceVector = displacement;

			//Handle collision with other components
			if (sweepMoveHit.GetActor())
			{
				auto moveComp = sweepMoveHit.GetActor()->GetComponentByClass<UModularControllerComponent>();
				if (moveComp && sweepMoveHit.Component == moveComp->UpdatedPrimitive)
				{
					const FVector force = UFunctionLibrary::GetKineticEnergy(finalKcomp.LinearKinematic.Velocity, GetMass(), displacement.Length()).ProjectOnToNormal(hitNormal);
					moveComp->AddForce(force * InterCollisionForceScale);
				}
			}
		}

		//Debug the move
		if (DebugType == ControllerDebugType_MovementDebug)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, initialLocation, endLocation, 15
			                                     , blockingHit ? FColor::Orange : FColor::Green, delta * 2);
		}
	}


	//Analytic Debug
	if (DebugType == ControllerDebugType_MovementDebug)
	{
		const FVector relativeVel = finalKcomp.LinearKinematic.Velocity - finalKcomp.LinearKinematic.refVelocity;
		const FVector relativeAcc = finalKcomp.LinearKinematic.Acceleration - finalKcomp.LinearKinematic.refAcceleration;

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Referential Movement: Vel[Dir:(%s), Lenght:(%f) m/s], Acc[Dir:(%s) Lenght:(%f) m/s2]")
		                                                        , *finalKcomp.LinearKinematic.refVelocity.GetSafeNormal().ToCompactString(),
		                                                        finalKcomp.LinearKinematic.refVelocity.Length() * 0.01,
		                                                        *finalKcomp.LinearKinematic.refAcceleration.GetSafeNormal().ToCompactString(),
		                                                        finalKcomp.LinearKinematic.refAcceleration.Length() * 0.01), true, false, FColor::Magenta, 60, "refInfos");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Global Position: (%s)"), *finalKcomp.LinearKinematic.Position.ToCompactString())
		                                  , true, false, FColor::Blue, 60, "Pos");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Linear Velocity [ Global {Dir:(%s), Lenght:(%f) m/s} | Relative {Dir:(%s), Lenght:(%f) m/s}]")
		                                                        , *finalKcomp.LinearKinematic.Velocity.GetSafeNormal().ToCompactString(),
		                                                        (finalKcomp.LinearKinematic.Velocity.Length() * 0.01),
		                                                        *relativeVel.GetSafeNormal().ToCompactString(), relativeVel.Length() * 0.01), true, false, FColor::Cyan, 60, "LineSpd");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Linear Acceleration [ Global {Dir:(%s), Lenght:(%f) m/s2} | Relative {Dir:(%s), Lenght:(%f) m/s2}]")
		                                                        , *finalKcomp.LinearKinematic.Acceleration.GetSafeNormal().ToCompactString(),
		                                                        (finalKcomp.LinearKinematic.Acceleration.Length() * 0.01),
		                                                        *relativeAcc.GetSafeNormal().ToCompactString(),
		                                                        relativeAcc.Length() * 0.01), true, false, FColor::Purple, 60, "lineAcc");

		if (finalKcomp.LinearKinematic.Acceleration.SquaredLength() > 0)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, finalKcomp.LinearKinematic.Position,
			                                     finalKcomp.LinearKinematic.Position + finalKcomp.LinearKinematic.Acceleration.GetClampedToMaxSize(100) * 0.5, 50
			                                     , FColor::Purple, delta * 1.2, 3);
		}

		if (finalKcomp.LinearKinematic.Velocity.SquaredLength() > 0)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, finalKcomp.LinearKinematic.Position
			                                     , finalKcomp.LinearKinematic.Position + finalKcomp.LinearKinematic.Velocity.GetClampedToMaxSize(100) * 0.5,
			                                     50, FColor::Cyan, delta * 1.2, 3);
		}
	}

	return finalKcomp;
}


FControllerStatus UModularControllerComponent::ConsumeLastKinematicMove(FVector moveInput, float delta) const
{
	//Consume kinematics
	FControllerStatus initialState = ApplyedControllerStatus;
	initialState.Kinematics.LinearKinematic.Acceleration = FVector(0);
	initialState.Kinematics.LinearKinematic.CompositeMovements.Empty();
	initialState.Kinematics.LinearKinematic.refAcceleration = FVector(0);
	initialState.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	initialState.Kinematics.LinearKinematic.Position = GetLocation();
	initialState.Kinematics.AngularKinematic.AngularAcceleration = FVector(0);
	initialState.Kinematics.AngularKinematic.Orientation = GetRotation();
	initialState.MoveInput = moveInput;
	initialState.CustomPhysicDrag = -1;
	initialState.StatusParams.PrimaryActionFlag = 0;
	initialState.Kinematics.SurfaceBinaryFlag = -1;
	initialState.CustomSolverCheckDirection = FVector(0);
	if (initialState.Kinematics.LastMoveHit.HitResult.IsValidBlockingHit())
	{
		const FVector normal = initialState.Kinematics.LastMoveHit.HitResult.Normal;
		const FVector velocityAlongSurface = FVector::VectorPlaneProject(initialState.Kinematics.LinearKinematic.Velocity, normal);
		const FVector displacement = initialState.Kinematics.LastMoveHit.HitResult.Location - initialState.Kinematics.LastMoveHit.HitResult.TraceStart;
		initialState.Kinematics.LinearKinematic.Velocity = UFunctionLibrary::IsValidSurfaces(initialState.Kinematics)
			                                                   ? UFunctionLibrary::GetVelocityFromReaction(initialState.Kinematics, initialState.Kinematics.LinearKinematic.Velocity, false,
			                                                                                               ECR_Block)
			                                                   : velocityAlongSurface;
	}
	initialState.Kinematics.LastMoveHit = FHitResultExpanded();
	//initialState.StatusParams.StatusAdditionalCheckVariables.Empty();

	return initialState;
}


void UModularControllerComponent::KinematicPostMove(FControllerStatus newStatus, const float inDelta)
{
	Velocity = newStatus.Kinematics.LinearKinematic.Velocity;
	ApplyedControllerStatus = newStatus;
	UpdateComponentVelocity();
}


FAngularKinematicCondition UModularControllerComponent::HandleKinematicRotation(const FKinematicComponents inKinematic, const float inDelta) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("HandleKinematicRotation");
	FAngularKinematicCondition outputCondition = inKinematic.AngularKinematic;

	FVector gravityUp = -GetGravityDirection();

	//Handle surfaces rotation
	{
		const FVector surfaceRotVel = UFunctionLibrary::GetAverageSurfaceAngularSpeed(inKinematic);
		outputCondition.Orientation *= FQuat(surfaceRotVel.GetSafeNormal(), FMath::DegreesToRadians(surfaceRotVel.Length()) * inDelta);
	}

	//Acceleration
	outputCondition.AngularAcceleration = outputCondition.AngularAcceleration.ProjectOnToNormal(gravityUp);

	//Rotation
	outputCondition.RotationSpeed = outputCondition.RotationSpeed.ProjectOnToNormal(gravityUp);

	//Orientation
	{
		FVector virtualFwdDir = FVector::VectorPlaneProject(outputCondition.Orientation.Vector(), gravityUp);
		FVector virtualRightDir = FVector::ZeroVector;
		if (virtualFwdDir.Normalize())
		{
			virtualRightDir = FVector::CrossProduct(gravityUp, virtualFwdDir);
		}
		else
		{
			virtualFwdDir = -virtualFwdDir.Rotation().Quaternion().GetAxisZ();
			FVector::CreateOrthonormalBasis(virtualFwdDir, virtualRightDir, gravityUp);
			virtualFwdDir.Normalize();
		}
		if (!virtualRightDir.Normalize())
		{
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::PrintString(
					this, FString::Printf(TEXT("Cannot normalize right vector: up = %s, fwd= %s"), *gravityUp.ToCompactString(), *virtualFwdDir.ToCompactString()), true,
					false, FColor::Yellow, inDelta * 2, "RotError");
			}
			return outputCondition;
		}
		FRotator desiredRotator = UKismetMathLibrary::MakeRotFromZX(gravityUp, virtualFwdDir);

		virtualFwdDir = desiredRotator.Quaternion().GetAxisX();
		virtualRightDir = desiredRotator.Quaternion().GetAxisY();

		FQuat targetQuat = desiredRotator.Quaternion() * RotationOffset.Quaternion();

		outputCondition.Orientation = targetQuat;
	}

	//Update
	outputCondition = outputCondition.GetFinalCondition(inDelta);

	//Debug
	{
		if (DebugType == ControllerDebugType_MovementDebug)
		{
			auto acc = outputCondition.AngularAcceleration;

			auto spd = outputCondition.GetAngularSpeedQuat();
			FVector spdAxis;
			float spdAngle;
			spd.ToAxisAndAngle(spdAxis, spdAngle);

			auto rot = outputCondition.Orientation;
			FVector rotAxis;
			float rotAngle;
			rot.ToAxisAndAngle(rotAxis, rotAngle);


			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Rotation [ Axis: (%s), Angle: (%f)]"), *rotAxis.ToCompactString(), FMath::RadiansToDegrees(rotAngle)), true, false,
			                                  FColor::Yellow, inDelta * 2, "Rot");
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Angular Velocity [ Axis: (%s), Angle: (%f)]"), *spdAxis.ToCompactString(), FMath::RadiansToDegrees(spdAngle)),
			                                  false,
			                                  true,
			                                  FColor::Orange, inDelta * 2, "Spd");
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Angular Acceleration [ Axis: (%s), Angle: (%f)]"), *acc.GetSafeNormal().ToCompactString(), acc.Length()), false,
			                                  true,
			                                  FColor::Red, inDelta * 2, "Acc");
		}
	}

	return outputCondition;
}


FVector UModularControllerComponent::SlideAlongSurfaceAt(FHitResult& Hit, const FQuat rotation, FVector attemptedMove, int& depth, double deltaTime, float hullInflation)
{
	FVector initialLocation = Hit.Location;
	FVector endLocation = initialLocation;
	FVector originalMove = attemptedMove;
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(GetOwner());
	const float offsetter = 1;//FMath::Abs(hullInflation);
	const float inflatter = FMath::Abs(hullInflation);

	//Compute slide vector
	FVector slideMove = ComputeSlideVector(originalMove, 1 - Hit.Time, Hit.Normal, Hit);

	if (DebugType == ControllerDebugType_MovementDebug)
	{
		UFunctionLibrary::DrawDebugCircleOnHit(Hit, false, 43 + depth * 5, FColor::Green, deltaTime * 2, 1, false);
		UKismetSystemLibrary::DrawDebugArrow(this, Hit.ImpactPoint, Hit.ImpactPoint + slideMove / deltaTime, 50, FColor::Green, deltaTime * 1.2);
	}

	if ((slideMove | originalMove) > 0.f)
	{
		FHitResult primaryHit;
		const FVector primaryOffset = Hit.Normal * offsetter;

		//Check primary
		if (ComponentTraceCastSingle_Internal(primaryHit, initialLocation + primaryOffset, slideMove + slideMove.GetSafeNormal() * inflatter, rotation, hullInflation - inflatter,
		                                      bUseComplexCollision,
		                                      queryParams))
		{
			// Compute new slide normal when hitting multiple surfaces.
			FVector firstHitLocation = primaryHit.Location - primaryHit.Normal * inflatter;
			FVector twoWallAdjust = originalMove;// * (1 - Hit.Time);
			TwoWallAdjust(twoWallAdjust, primaryHit, Hit.Normal);

			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UFunctionLibrary::DrawDebugCircleOnHit(primaryHit, false, 38 + depth * 5, primaryHit.bStartPenetrating ? FColor::Red : FColor::Yellow, deltaTime * 1.2, 1, false);
				UKismetSystemLibrary::DrawDebugArrow(this, primaryHit.ImpactPoint, primaryHit.ImpactPoint + twoWallAdjust / deltaTime, 50,
				                                     primaryHit.bStartPenetrating ? FColor::Red : FColor::Yellow, deltaTime * 1.2, 5);
			}

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if ((twoWallAdjust | originalMove) > 0.f)
			{
				FHitResult secondaryMove;
				const FVector newNormal_rht = (Hit.ImpactPoint - primaryHit.ImpactPoint).GetSafeNormal();
				const FVector newNormal = FVector::VectorPlaneProject(Hit.Normal + primaryHit.Normal, newNormal_rht).GetSafeNormal();
				const FVector secondaryOffset = newNormal * offsetter;

				// Perform second move
				if (ComponentTraceCastSingle_Internal(secondaryMove, firstHitLocation + secondaryOffset, twoWallAdjust + twoWallAdjust.GetSafeNormal() * 2 * inflatter, rotation,
				                                      hullInflation - 2 * inflatter, bUseComplexCollision, queryParams))
				{
					if (DebugType == ControllerDebugType_MovementDebug)
					{
						UFunctionLibrary::DrawDebugCircleOnHit(secondaryMove, false, 33 + depth * 5, secondaryMove.bStartPenetrating ? FColor::Black : FColor::Purple, deltaTime * 1.2, 1,
						                                       false);
					}

					if (depth > 0)
					{
						depth--;
						secondaryMove.Location -= secondaryOffset + secondaryMove.Normal * 2 * inflatter;
						endLocation = SlideAlongSurfaceAt(secondaryMove, rotation, twoWallAdjust, depth, deltaTime, hullInflation);
						return endLocation;
					}
					else
					{
						endLocation = secondaryMove.Location - secondaryOffset - secondaryMove.Normal * 2 * inflatter;

						if (DebugType == ControllerDebugType_MovementDebug)
						{
							if (DebugType == ControllerDebugType_MovementDebug)
							{
								FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
								midPoint = midPoint + (secondaryMove.ImpactPoint - midPoint) * 0.5;
								UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + (newNormal * twoWallAdjust.Length()) / deltaTime, 50,
								                                     secondaryMove.bStartPenetrating ? FColor::Black : FColor::Purple, deltaTime * 2);
							}
						}

						return endLocation;
					}
				}
				else
				{
					endLocation = firstHitLocation - primaryOffset + twoWallAdjust;
					if (DebugType == ControllerDebugType_MovementDebug)
					{
						FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
						UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + twoWallAdjust / deltaTime, 50, FColor::Orange, deltaTime * 1.2);
					}
					return endLocation;
				}
			}
			else
			{
				endLocation = firstHitLocation - primaryOffset;
				return endLocation;
			}
		}
		else
		{
			endLocation = initialLocation + slideMove;
			return endLocation;
		}
	}
	else
	{
		return initialLocation;
	}
}


#pragma endregion
