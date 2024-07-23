// Copyright � 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "CommonTypes.h"
#include "Engine/HitResult.h"
#include "FunctionLibrary.generated.h"


//Common types function library
UCLASS(BlueprintType)
class MODULARCONTROLLER_API UFunctionLibrary : public UObject
{
	GENERATED_BODY()

public:
	UFunctionLibrary();

	// Animation Utilities ///////////////////////////////////////////////////////////////////////

	static void ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose);

	static void ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose);

	static FTransform ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime);

	static FTransform ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time);

	// Get the weight of the montage specified or of the active montage if NULL Montage.
	UFUNCTION(BlueprintPure, Category = "Function Library | Animation")
	static double GetMontageCurrentWeight(const UAnimInstance* AnimInstance, const UAnimMontage* Montage);


	//-///////////////////////////////////////////////////////////////////////////////////////


	// Get the relative 3D direction from a 2D vector.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Inputs")
	static FVector GetAxisRelativeDirection(FVector2D input, FTransform transformRelative, FVector planeNormal = FVector(0));


	// Compare 2 collision shapes and return true if they are of same type and size.
	static bool CollisionShapeEquals(const FCollisionShape shapeA, const FCollisionShape shapeB);


	// Get surface friction (X), surface bounciness (Y)
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface | Physic")
	static FVector GetSurfacePhysicProperties(const FHitResult MyStructRef);

	// Get the resulting surface friction (X), surface bounciness (Y) by mixing PhysicMaterial and Base properties by the rules defined by PhysicMaterial
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface | Physic")
	static FVector GetMixedPhysicProperties(const FHitResult MyStructRef, const FVector Base);


	// Draw a debug circle at the hit point on a surface.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface | Debug", meta=(AdvancedDisplay=1))
	static void DrawDebugCircleOnHit(const FHitResult MyStructRef, float radius = 40, FLinearColor color = FLinearColor::White, float duration = 0, float thickness = 1,
	                                 bool showImpactAxis = false);


	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface | Debug", meta=(AdvancedDisplay=1))
	static void DrawDebugCircleOnSurface(const FSurface MyStructRef, float radius = 40, FLinearColor color = FLinearColor::White, float duration = 0, float thickness = 1,
	                                     bool showImpactAxis = false);


	// Get the point to the object T from a soft object pointer.
	template <typename T>
	UFUNCTION(BlueprintCallable, Category = "Function Library | Common Objects")
	static std::enable_if_t<std::is_base_of_v<UObject, T>, T> GetObject(const TSoftObjectPtr<T> softObj);

	// Turn toward a direction form angular kinematics.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Transform Tools")
	static FAngularKinematicCondition LookAt(const FAngularKinematicCondition startCondition, const FVector direction, const float withSpeed, const float deltaTime);

	// Linear and spherical interpolation between two kinematics.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FKinematicComponents LerpKinematic(const FKinematicComponents A, const FKinematicComponents B, const double delta);

	// Get the Kinetic Energy of a body of mass (Kg), traveling at velocity. Set distance travelled to get the force.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FVector GetKineticEnergy(const FVector velocity, const float mass, const double distanceTraveled = 1);

	//Get the vector to snap a shape point on a surface along an axis.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FVector GetSnapOnSurfaceVector(const FVector onShapeTargetSnapPoint, const FSurface Surface, const FVector SnapAxis);

	// Add or replace a key value pair in StatusAdditionalCheckVariables. return true if the key is new.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Status Parameters")
	static bool AddOrReplaceCosmeticVariable(FStatusParameters& statusParam, const FName key, const float value);

	// Add or replace a Vector key value pair in StatusAdditionalCheckVariables. return true if the key is new.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Status Parameters")
	static bool AddOrReplaceCosmeticVector(FStatusParameters& statusParam, const FName key, const FVector value);

	// Get variable value from StatusAdditionalCheckVariables at key.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Status Parameters")
	static float GetCosmeticVariable(FStatusParameters statusParam, const FName key, const float notFoundValue = 0);

	// Get vector value from StatusAdditionalCheckVariables at key.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Status Parameters")
	static FVector GetCosmeticVector(FStatusParameters statusParam, const FName key, const FVector notFoundValue = FVector(0));

	//Set the referential movement (usually the surface the controller is on)
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics | Linear")
	static void SetReferentialMovement(FLinearKinematicCondition& linearKinematic, const FVector movement, const float delta, const float acceleration);

	//Add a composite movement. useful to match a certain speed.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics | Linear")
	static void AddCompositeMovement(FLinearKinematicCondition& linearKinematic, const FVector movement, const float acceleration, int index);

	//Remove a composite movement at index.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics | Linear")
	static bool RemoveCompositeMovement(FLinearKinematicCondition& linearKinematic, int index);

	// Get the linear velocity relative to the surfaces affecting his move.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FVector GetRelativeVelocity(FKinematicComponents kinematicComponent, const float deltaTime, ECollisionResponse channelFilter = ECR_MAX);

	// Apply force on the set of surfaces in contact defined by SurfaceBinaryFlag.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static void ApplyForceOnSurfaces(FKinematicComponents& kinematicComponent, const FVector point, const FVector force, bool reactionForce = false,
	                                 ECollisionResponse channelFilter = ECR_MAX);

	// Get the velocity along surfaces in contact defined by SurfaceBinaryFlag.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FVector GetVelocityFromReaction(FKinematicComponents kinematicComponent, const FVector velocity, const bool useImpactNormal = false, ECollisionResponse channelFilter = ECR_MAX);

	// Get the average surface velocity from a the set of surfaces in contact defined by SurfaceBinaryFlag
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FVector GetAverageSurfaceVelocityAt(FKinematicComponents kinematicComponent, const FVector point, const float deltaTime = 0, ECollisionResponse channelFilter = ECR_MAX);

	// Get the rotation diff as Axis, angle (Deg/sec) from the set of surfaces in contact defined by SurfaceBinaryFlag
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FVector GetAverageSurfaceAngularSpeed(FKinematicComponents kinematicComponent, ECollisionResponse channelFilter = ECR_MAX);

	// Get the combine properties (Max friction, Max bounciness, Blockiest response type) from the set of surfaces in contact defined by SurfaceBinaryFlag
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FVector GetMaxSurfacePhysicProperties(FKinematicComponents kinematicComponent, ECollisionResponse channelFilter = ECR_MAX);

	// Check if at least one of the surfaces defined in SurfaceBinaryFlag is valid.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static bool IsValidSurfaces(FKinematicComponents kinematicComponent, ECollisionResponse channelFilter = ECR_MAX);

	// Get an action montage at index in montage library
	UFUNCTION(BlueprintCallable, Category = "Function Library | Actions")
	static FActionMotionMontage GetActionMontageAt(FActionMontageLibrary& structRef, int index, int fallbackIndex = 0, bool tryZeroOnNoFallBack = false);

	// Get an action montage by sixaxis direction in montage library
	UFUNCTION(BlueprintCallable, Category = "Function Library | Actions")
	static FActionMotionMontage GetActionMontageInDirection(FActionMontageLibrary& structRef, ESixAxisDirectionType direction,
	                                                        ESixAxisDirectionType fallbackDirection = ESixAxisDirectionType::Forward);

	// Get the max weight of montages in the library
	UFUNCTION(BlueprintCallable, Category = "Function Library | Actions")
	static double GetActionLibraryMontageMaxWeight(FActionMontageLibrary& structRef, const UAnimInstance* AnimInstance);

	// Get the index of the first surface matching condition
	static int GetSurfaceIndexUnderCondition(FKinematicComponents kinematicComponent, std::function<bool(FSurface&)> condition);


	// Compute the final velocities of two colliding objects A and B, and return true if the operation succeeded.
	static bool ComputeCollisionVelocities(const FVector initialVelA, const FVector initialVelB, const FVector colNornal, const double massA, const double massB
	                                       , const double bounceCoef, FVector& finalA, FVector& finalB);

	//Get the Mass of the component upon hit
	static double GetHitComponentMass(FHitResult hit);
};
