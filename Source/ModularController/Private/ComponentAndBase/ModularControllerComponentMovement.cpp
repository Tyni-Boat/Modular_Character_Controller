// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "CollisionQueryParams.h"




#pragma region Movement XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void UModularControllerComponent::Move_Implementation(const FKinematicComponents finalKinematic, float deltaTime)
{
	if (!UpdatedPrimitive)
		return;

	FVector posOffset = GetLocation() - _lastLocation;
	FVector offset = finalKinematic.LinearKinematic.Position - GetLocation();// initialKinematic.LinearKinematic.Position;
	FQuat rotOffset = finalKinematic.AngularKinematic.Orientation * GetRotation().Inverse();// initialKinematic.AngularKinematic.Orientation.Inverse();

	if (UpdatedPrimitive->IsSimulatingPhysics())
	{
		UpdatedPrimitive->SetAllPhysicsPosition(finalKinematic.LinearKinematic.Position);
		UpdatedPrimitive->SetAllPhysicsLinearVelocity(finalKinematic.LinearKinematic.Velocity);
		UpdatedPrimitive->SetAllPhysicsRotation(finalKinematic.AngularKinematic.Orientation);
	}
	else
	{
		//UpdatedPrimitive->SetWorldRotation(finalKinematic.AngularKinematic.Orientation, false);
		UpdatedPrimitive->SetWorldLocation(finalKinematic.LinearKinematic.Position + posOffset, false);
		//UpdatedPrimitive->AddWorldRotation(rotOffset);
		//UpdatedPrimitive->AddWorldOffset(offset);
	}
}


FKinematicComponents UModularControllerComponent::KinematicMoveEvaluation(FControllerStatus processedMove, bool noCollision, float delta, bool applyForceOnSurfaces)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("KinematicMoveEvaluation");
	FKinematicComponents finalKcomp = FKinematicComponents(processedMove.Kinematics);
	FVector initialLocation = processedMove.Kinematics.LinearKinematic.Position;
	FVector startingLocation = processedMove.Kinematics.LinearKinematic.Position;
	const FQuat primaryRotation = processedMove.Kinematics.GetRotation();
	FVector acceleration = processedMove.Kinematics.LinearKinematic.Acceleration;
	FQuat surfaceRotRate = FQuat::Identity;
	constexpr float hull = 0.75;
	const double drag = processedMove.CustomPhysicProperties.Y >= 0 ? processedMove.CustomPhysicProperties.Y : Drag;

	//Drag
	FVector vel = finalKcomp.LinearKinematic.Velocity * 0.01;
	const double velSqr = vel.SquaredLength();
	if (vel.Normalize())
	{
		acceleration -= vel * ((velSqr * drag) / (2 * GetMass())) * 100;
	}

	//Penetration force and velocity
	if (!noCollision)
	{
		//FVector penetrationVec = FVector(0);
		//FVector contactForce = FVector(0);
		//if (CheckPenetrationAt(penetrationVec, contactForce, initialLocation, primaryRotation, nullptr, hull, true))
		//{
		//	FVector forceDirection = contactForce;
		//	initialLocation += penetrationVec;

		//	if (forceDirection.Normalize() && penetrationVec.Normalize())
		//	{
		//		if ((forceDirection | penetrationVec) > 0)
		//		{
		//			//acceleration += contactForce / GetMass();
		//		}
		//	}
		//}
	}

	//Handle surface Operations if any
	if (processedMove.ControllerSurface.GetSurfacePrimitive())
	{
		FVector surfaceVel = processedMove.ControllerSurface.GetSurfaceLinearVelocity(true, true, false);
		const FVector surfaceValues = UFunctionLibrary::GetSurfacePhysicProperties(processedMove.ControllerSurface.GetHitResult());
		finalKcomp.LinearKinematic.SetReferentialMovement(surfaceVel, delta, processedMove.ControllerSurface.HadChangedSurface() ? 0 : (1 / delta) * surfaceValues.X);
		surfaceRotRate = processedMove.ControllerSurface.GetSurfaceAngularVelocity(true);
	}
	else
	{
		finalKcomp.LinearKinematic.SetReferentialMovement(FVector(0), delta, 0);
	}

	//Kinematic function to evaluate position and velocity from acceleration
	FVector selfVel = finalKcomp.LinearKinematic.Velocity;
	finalKcomp.LinearKinematic.Acceleration = acceleration;
	finalKcomp.LinearKinematic = finalKcomp.LinearKinematic.GetFinalCondition(delta);
	acceleration = finalKcomp.LinearKinematic.Acceleration;
	FVector refMotionVel = finalKcomp.LinearKinematic.refVelocity;

	//Force on surface	
	if (applyForceOnSurfaces)
	{
		if (processedMove.ControllerSurface.GetSurfacePrimitive())
		{
			FVector normalForce = (acceleration).ProjectOnToNormal(processedMove.ControllerSurface.GetHitResult().ImpactNormal) * GetMass();
			const auto surfacePrimo = processedMove.ControllerSurface.GetSurfacePrimitive();
			const FVector impactPt = processedMove.ControllerSurface.GetHitResult().ImpactPoint;
			const FVector impactNormal = processedMove.ControllerSurface.GetHitResult().ImpactNormal;
			const FName impactBoneName = processedMove.ControllerSurface.GetHitResult().BoneName;
			const FVector surfaceValues = UFunctionLibrary::GetSurfacePhysicProperties(processedMove.ControllerSurface.GetHitResult());

			FVector outSelfVel = FVector(0);
			FVector outSurfaceVel = FVector(0);

			if (surfacePrimo && surfacePrimo->IsSimulatingPhysics())
			{
				FVector atPtVelocity = surfacePrimo->GetPhysicsLinearVelocityAtPoint(impactPt, impactBoneName);
				const double surfaceMass = surfacePrimo->GetMass();
				//Landing
				if (processedMove.ControllerSurface.HadLandedOnSurface())
				{
					if ((selfVel | impactNormal) < 0
						&& UFunctionLibrary::ComputeCollisionVelocities(selfVel, atPtVelocity, impactNormal, GetMass(), surfaceMass, surfaceValues.Y, outSelfVel, outSurfaceVel))
					{
						const FVector forceOnSurface = (outSurfaceVel / delta) * GetMass();
						surfacePrimo->AddForceAtLocation(forceOnSurface, impactPt, impactBoneName);
					}
				}
				//Continuous
				else
				{
					surfacePrimo->AddForceAtLocation(normalForce * 0.01 + GetGravity() * GetMass(), impactPt, impactBoneName);
				}
			}
		}
	}


	//Primary Movement (momentum movement)
	{
		FHitResult sweepMoveHit = FHitResult(EForceInit::ForceInitToZero);
		FCollisionQueryParams queryParams;
		queryParams.AddIgnoredComponents(IgnoredCollisionComponents);
		FVector priMove = finalKcomp.LinearKinematic.Velocity;
		FVector conservedVelocity = finalKcomp.LinearKinematic.Velocity;

		//Snap displacement
		const FVector noSnapPriMove = priMove;
		priMove += finalKcomp.LinearKinematic.SnapDisplacement / delta;


		bool blockingHit = false;
		FVector endLocation = initialLocation;
		FVector moveDisplacement = FVector(0);

		//Trace to detect hit while moving
		blockingHit = noCollision ? false : ComponentTraceCastSingle_Internal(sweepMoveHit, initialLocation, priMove * delta, primaryRotation, hull, bUseComplexCollision, queryParams);

		//Try to adjust the referential move
		if (blockingHit && refMotionVel.SquaredLength() > 0)
		{
			priMove -= refMotionVel;
			conservedVelocity -= refMotionVel;
			refMotionVel = FVector::VectorPlaneProject(refMotionVel, sweepMoveHit.Normal);
			priMove += refMotionVel;
			conservedVelocity += refMotionVel;
			initialLocation = sweepMoveHit.Location + sweepMoveHit.Normal * 0.01;
			sweepMoveHit = FHitResult(EForceInit::ForceInitToZero);
			blockingHit = noCollision ? false : ComponentTraceCastSingle_Internal(sweepMoveHit, initialLocation, priMove * delta, primaryRotation, hull, bUseComplexCollision, queryParams);
		}
		else
		{
			finalKcomp.AngularKinematic.Orientation *= surfaceRotRate;
		}

		//Handle collision
		if (blockingHit)
		{
			//Properties of the hit surface (X=friction, Y=Bounciness, Z= hit component mass)
			const FVector surfaceProperties = UFunctionLibrary::GetSurfacePhysicProperties(sweepMoveHit);
			const double surfaceMass = GetHitComponentMass(sweepMoveHit);
			const double friction = processedMove.CustomPhysicProperties.X >= 0 ? processedMove.CustomPhysicProperties.X : surfaceProperties.X;
			//const double frictionVelocity = UFunctionLibrary::GetFrictionAcceleration(sweepMoveHit.Normal, acceleration * GetMass(), GetMass(), friction) * delta;
			const double bounciness = processedMove.CustomPhysicProperties.Z >= 0 ? processedMove.CustomPhysicProperties.Z : surfaceProperties.Y;

			//Reaction and Slide on surface
			const FVector pureReaction = -priMove.ProjectOnToNormal(sweepMoveHit.Normal);
			const FVector reactionVelocity = pureReaction * bounciness;
			const FVector frictionlessVelocity = FVector::VectorPlaneProject(priMove, sweepMoveHit.Normal);

			int maxDepth = 1;
			endLocation = SlideAlongSurfaceAt(sweepMoveHit, primaryRotation, (frictionlessVelocity - pureReaction) * delta, maxDepth, delta, hull);

			//Stuck protection
			if (sweepMoveHit.bStartPenetrating && initialLocation.Equals(endLocation, 0.35) && priMove.SquaredLength() > 0)
			{
				if (DebugType == ControllerDebugType_MovementDebug)
				{
					UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("I'm stuck: initial location: (%s). End location: (%s)"), *initialLocation.ToCompactString(), *endLocation.ToCompactString()), true, true, FColor::Magenta, delta * 2, "stuck");
					UKismetSystemLibrary::DrawDebugArrow(this, sweepMoveHit.ImpactPoint, sweepMoveHit.ImpactPoint + sweepMoveHit.Normal * 50, 50, FColor::Magenta, delta * 2, 3);
				}
				endLocation += sweepMoveHit.Normal * (sweepMoveHit.PenetrationDepth > 0 ? sweepMoveHit.PenetrationDepth : 0.125);
			}

			//Debug Movement
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Move Amount Done: (%f) percent. Initial overlap? (%d)"), sweepMoveHit.Time * 100, sweepMoveHit.bStartPenetrating), true, true, FColor::Red, delta, "hitTime");
			}

			//Handle positioning and Velocity
			moveDisplacement = endLocation - startingLocation;
			FVector displacementVector = moveDisplacement;
			if (displacementVector.Normalize())
			{
				conservedVelocity = FVector(0);// displacementVector* frictionlessVelocity.Length()* (1 - sweepMoveHit.Time);
			}
			else
			{
				conservedVelocity = FVector(0);// +reactionVelocity;
			}
			conservedVelocity = moveDisplacement / delta;
		}
		else
		{
			//Make the move
			moveDisplacement = priMove * delta;
			endLocation = initialLocation + moveDisplacement;

			//Debug the move
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::DrawDebugArrow(this, initialLocation, initialLocation + priMove * delta, 50, FColor::Green, delta * 2);
			}
		}

		//Compute final position ,velocity and acceleration
		const FVector primaryDelta = moveDisplacement;
		const FVector location = endLocation;
		finalKcomp.LinearKinematic = processedMove.Kinematics.LinearKinematic.GetFinalFromPosition(location, delta, false);
		finalKcomp.LinearKinematic.Acceleration = acceleration;
		finalKcomp.LinearKinematic.Velocity = conservedVelocity;
		finalKcomp.LinearKinematic.refVelocity = refMotionVel;
	}


	//Analytic Debug
	if (DebugType == ControllerDebugType_MovementDebug)
	{
		const FVector relativeVel = finalKcomp.LinearKinematic.Velocity - finalKcomp.LinearKinematic.refVelocity;
		const FVector relativeAcc = finalKcomp.LinearKinematic.Acceleration - finalKcomp.LinearKinematic.refAcceleration;

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Referential Movement: Vel[Dir:(%s), Lenght:(%f) m/s], Acc[Dir:(%s) Lenght:(%f) m/s2]"), *finalKcomp.LinearKinematic.refVelocity.GetSafeNormal().ToCompactString(), finalKcomp.LinearKinematic.refVelocity.Length() * 0.01, *finalKcomp.LinearKinematic.refAcceleration.GetSafeNormal().ToCompactString(), finalKcomp.LinearKinematic.refAcceleration.Length() * 0.01), true, true, FColor::Magenta, 60, "refInfos");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Global Position: (%s)"), *finalKcomp.LinearKinematic.Position.ToCompactString()), true, true, FColor::Blue, 60, "Pos");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Linear Velocity [ Global {Dir:(%s), Lenght:(%f) m/s} | Relative {Dir:(%s), Lenght:(%f) m/s}]"), *finalKcomp.LinearKinematic.Velocity.GetSafeNormal().ToCompactString(), (finalKcomp.LinearKinematic.Velocity.Length() * 0.01), *relativeVel.GetSafeNormal().ToCompactString(), relativeVel.Length() * 0.01), true, true, FColor::Cyan, 60, "LineSpd");

		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Linear Acceleration [ Global {Dir:(%s), Lenght:(%f) m/s2} | Relative {Dir:(%s), Lenght:(%f) m/s2}]"), *acceleration.GetSafeNormal().ToCompactString(), (acceleration.Length() * 0.01), *relativeAcc.GetSafeNormal().ToCompactString(), relativeAcc.Length() * 0.01), true, true, FColor::Purple, 60, "lineAcc");

		if (finalKcomp.LinearKinematic.Acceleration.SquaredLength() > 0)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, finalKcomp.LinearKinematic.Position, finalKcomp.LinearKinematic.Position + finalKcomp.LinearKinematic.Acceleration.GetClampedToMaxSize(100) * 0.5, 50, FColor::Purple, delta * 2, 3);
		}

		if (finalKcomp.LinearKinematic.Velocity.SquaredLength() > 0)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, finalKcomp.LinearKinematic.Position, finalKcomp.LinearKinematic.Position + finalKcomp.LinearKinematic.Velocity.GetClampedToMaxSize(100) * 0.5, 50, FColor::Cyan, delta * 2, 3);
		}
	}

	return finalKcomp;
}


FControllerStatus UModularControllerComponent::ConsumeLastKinematicMove(FVector moveInput) const
{
	//Consume kinematics
	FControllerStatus initialState = ApplyedControllerStatus;
	initialState.Kinematics.LinearKinematic.Acceleration = FVector(0);
	//initialState.Kinematics.LinearKinematic.Velocity = FVector(0);
	initialState.Kinematics.LinearKinematic.CompositeMovements.Empty();
	initialState.Kinematics.LinearKinematic.refAcceleration = FVector(0);
	initialState.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	initialState.Kinematics.LinearKinematic.Position = GetLocation();
	initialState.Kinematics.AngularKinematic.AngularAcceleration = FVector(0);
	initialState.Kinematics.AngularKinematic.Orientation = GetRotation();
	initialState.MoveInput = moveInput;
	initialState.CustomPhysicProperties = FVector(-1);
	initialState.StatusParams.PrimaryActionFlag = 0;

	initialState.SurfaceIndex = -1;

	return initialState;
}


void UModularControllerComponent::KinematicPostMove(FControllerStatus newStatus, const float inDelta)
{
	Velocity = newStatus.Kinematics.LinearKinematic.Velocity;
	ApplyedControllerStatus = newStatus;
	UpdateComponentVelocity();
}


FAngularKinematicCondition UModularControllerComponent::HandleKinematicRotation(const FAngularKinematicCondition inRotCondition, const float inDelta) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("HandleKinematicRotation");
	FAngularKinematicCondition outputCondition = inRotCondition;

	FVector gravityUp = -GetGravityDirection();

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
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Cannot normalize right vector: up = %s, fwd= %s"), *gravityUp.ToCompactString(), *virtualFwdDir.ToCompactString()), true, true, FColor::Yellow, inDelta * 2, "RotError");
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


			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Rotation [ Axis: (%s), Angle: (%f)]"), *rotAxis.ToCompactString(), FMath::RadiansToDegrees(rotAngle)), true, true, FColor::Yellow, inDelta * 2, "Rot");
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Angular Velocity [ Axis: (%s), Angle: (%f)]"), *spdAxis.ToCompactString(), FMath::RadiansToDegrees(spdAngle)), true, true, FColor::Orange, inDelta * 2, "Spd");
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Angular Acceleration [ Axis: (%s), Angle: (%f)]"), *acc.GetSafeNormal().ToCompactString(), acc.Length()), true, true, FColor::Red, inDelta * 2, "Acc");
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
	queryParams.AddIgnoredComponent(Hit.GetComponent());

	//Compute slide vector
	FVector slideMove = ComputeSlideVector(originalMove, 1 - Hit.Time, Hit.Normal, Hit);

	if (DebugType == ControllerDebugType_MovementDebug)
	{
		UFunctionLibrary::DrawDebugCircleOnSurface(Hit, false, 43 + depth * 5, FColor::Green, deltaTime * 2, 1, false);
	}

	if ((slideMove | originalMove) > 0.f)
	{
		FHitResult primaryHit;

		//Check primary
		if (ComponentTraceCastSingle_Internal(primaryHit, initialLocation + Hit.Normal * 0.001, slideMove, rotation, hullInflation, bUseComplexCollision, queryParams))
		{
			queryParams.AddIgnoredComponent(primaryHit.GetComponent());

			// Compute new slide normal when hitting multiple surfaces.
			FVector firstHitLocation = primaryHit.Location - Hit.Normal * 0.001;
			FVector twoWallAdjust = originalMove * (1 - Hit.Time);
			TwoWallAdjust(twoWallAdjust, primaryHit, Hit.ImpactNormal);

			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UFunctionLibrary::DrawDebugCircleOnSurface(primaryHit, false, 38 + depth * 5, FColor::Orange, deltaTime * 2, 1, false);
			}

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if ((twoWallAdjust | originalMove) > 0.f)
			{
				FHitResult secondaryMove;
				const FVector newNormal_rht = (Hit.ImpactPoint - primaryHit.ImpactPoint).GetSafeNormal();
				const FVector newNormal = FVector::VectorPlaneProject(Hit.Normal + primaryHit.Normal, newNormal_rht).GetSafeNormal();

				// Perform second move
				if (ComponentTraceCastSingle_Internal(secondaryMove, firstHitLocation + newNormal * 0.001, twoWallAdjust, rotation, hullInflation, bUseComplexCollision, queryParams))
				{
					queryParams.AddIgnoredComponent(secondaryMove.GetComponent());

					if (DebugType == ControllerDebugType_MovementDebug)
					{
						UFunctionLibrary::DrawDebugCircleOnSurface(secondaryMove, false, 33 + depth * 5, FColor::Black, deltaTime * 2, 1, false);
					}

					if (depth > 0)
					{
						depth--;
						secondaryMove.Location -= newNormal * 0.001;
						endLocation = SlideAlongSurfaceAt(secondaryMove, rotation, twoWallAdjust, depth, deltaTime, hullInflation);
						return endLocation;
					}
					else
					{
						endLocation = secondaryMove.Location - newNormal * 0.001;

						if (DebugType == ControllerDebugType_MovementDebug)
						{
							if (DebugType == ControllerDebugType_MovementDebug)
							{
								FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
								midPoint = midPoint + (secondaryMove.ImpactPoint - midPoint) * 0.5;
								UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + (newNormal * twoWallAdjust.Length()) / deltaTime, 50, FColor::Black, deltaTime * 2);
							}
						}

						return endLocation;
					}
				}
				else
				{
					endLocation = firstHitLocation + twoWallAdjust;
					if (DebugType == ControllerDebugType_MovementDebug)
					{
						FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
						UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + twoWallAdjust / deltaTime, 50, FColor::Orange, deltaTime * 2);
					}
					return endLocation;
				}
			}
			else
			{
				endLocation = firstHitLocation;
				if (DebugType == ControllerDebugType_MovementDebug)
				{
					FVector midPoint = primaryHit.ImpactPoint + (Hit.ImpactPoint - primaryHit.ImpactPoint) * 0.5;
					UKismetSystemLibrary::DrawDebugArrow(this, midPoint, midPoint + twoWallAdjust / deltaTime, 50, FColor::Yellow, deltaTime * 2);
				}
				return endLocation;
			}

		}
		else
		{
			endLocation = initialLocation + slideMove;
			if (DebugType == ControllerDebugType_MovementDebug)
			{
				UKismetSystemLibrary::DrawDebugArrow(this, initialLocation, initialLocation + slideMove / deltaTime, 50, FColor::Green, deltaTime * 2);
			}
			return endLocation;
		}
	}
	else
	{
		if (DebugType == ControllerDebugType_MovementDebug)
		{
			UKismetSystemLibrary::DrawDebugArrow(this, initialLocation, initialLocation + slideMove / deltaTime, 50, FColor::Cyan, deltaTime * 2);
		}
		return initialLocation;
	}
}


#pragma endregion

