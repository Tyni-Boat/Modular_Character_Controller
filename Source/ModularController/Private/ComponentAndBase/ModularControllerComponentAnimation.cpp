// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Engine/EngineTypes.h"


#pragma region Animation Component


FVector UModularControllerComponent::GetRootMotionVector() const
{
	return _RootMotionParams.GetTranslation() * RootMotionScale;
}


FQuat UModularControllerComponent::GetRootMotionQuat() const
{
	return _RootMotionParams.GetRotation();
}


TSoftObjectPtr<USkeletalMeshComponent> UModularControllerComponent::GetSkeletalMesh()
{
	if (!_skeletalMesh.IsValid())
	{
		UActorComponent* actorComp = MainSkeletal.GetComponent(GetOwner());
		_skeletalMesh = actorComp ? Cast<USkeletalMeshComponent>(actorComp) : nullptr;
	}
	return _skeletalMesh;
}


double UModularControllerComponent::PlayAnimationMontage_Internal(FActionMotionMontage Montage, float customAnimStartTime
                                                                  , bool useMontageEndCallback, FOnMontageEnded endCallBack)
{
	if (UAnimInstance* animInstance = GetAnimInstance())
	{
		return PlayAnimMontageSingle(animInstance, Montage, customAnimStartTime, useMontageEndCallback, endCallBack);
	}

	return -1;
}


double UModularControllerComponent::PlayAnimationMontageOnState_Internal(FActionMotionMontage Montage, FName stateName, float customAnimStartTime
                                                                         , bool useMontageEndCallback, FOnMontageEnded endCallBack)
{
	if (UAnimInstance* animInstance = GetAnimInstance(stateName))
	{
		return PlayAnimMontageSingle(animInstance, Montage, customAnimStartTime, useMontageEndCallback, endCallBack);
	}

	return -1;
}


double UModularControllerComponent::PlayAnimationMontage(FActionMotionMontage Montage, float customAnimStartTime)
{
	return PlayAnimationMontage_Internal(Montage, customAnimStartTime);
}


void UModularControllerComponent::StopMontage(FActionMotionMontage montage, bool isPlayingOnState)
{
	if (isPlayingOnState)
	{
		if (const auto currentState = GetCurrentControllerState())
		{
			if (auto* animInstance = GetAnimInstance(currentState->GetDescriptionName()))
			{
				if (animInstance->Montage_IsPlaying(montage.Montage))
				{
					animInstance->Montage_Stop(montage.Montage->BlendOut.GetBlendTime(), montage.Montage);
				}
			}
		}
	}
	else
	{
		if (auto* animInstance = GetAnimInstance())
		{
			if (animInstance->Montage_IsPlaying(montage.Montage))
			{
				animInstance->Montage_Stop(montage.Montage->BlendOut.GetBlendTime(), montage.Montage);
			}
		}
	}
}


double UModularControllerComponent::PlayAnimationMontageOnState(FActionMotionMontage Montage, FName stateName, float customAnimStartTime)
{
	return PlayAnimationMontageOnState_Internal(Montage, stateName, customAnimStartTime);
}


UAnimInstance* UModularControllerComponent::GetAnimInstance(FName stateName)
{
	if (GetSkeletalMesh().IsValid())
	{
		if (!stateName.IsNone())
		{
			const UBaseControllerState* state = GetControllerStateByName(stateName);
			if (state == nullptr)
				return nullptr;
			if (state->StateFallbackBlueprintClass == nullptr)
				return nullptr;
			return GetSkeletalMesh()->GetLinkedAnimLayerInstanceByClass(state->StateFallbackBlueprintClass);
		}
		return GetSkeletalMesh()->GetAnimInstance();
	}
	return nullptr;
}


void UModularControllerComponent::AddOrUpdateMotionWarp(FName warpKey, const FTransform inTransform)
{
	if (_motionWarpTransforms.Contains(warpKey))
		_motionWarpTransforms[warpKey] = inTransform;
	else
		_motionWarpTransforms.Add(warpKey, inTransform);
}

void UModularControllerComponent::RemoveMotionWarp(FName warpKey)
{
	if (_motionWarpTransforms.Contains(warpKey))
		_motionWarpTransforms.Remove(warpKey);
}

bool UModularControllerComponent::TryGetMotionWarpTransform(FName warpKey, FTransform& outTransform)
{
	bool result = false;
	if (_motionWarpTransforms.Contains(warpKey))
	{
		result = true;
		outTransform = _motionWarpTransforms[warpKey];
	}
	return result;
}


void UModularControllerComponent::LinkAnimBlueprint(TSoftObjectPtr<USkeletalMeshComponent> skeletalMeshReference, FName key, TSubclassOf<UAnimInstance> animClass)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("LinkAnimBlueprint");
	if (!skeletalMeshReference.IsValid())
		return;

	FQuat lookDir = skeletalMeshReference->GetComponentRotation().Quaternion();


	//The mesh is not Listed.
	if (!_linkedAnimClasses.Contains(skeletalMeshReference))
	{
		TMap<FName, TWeakObjectPtr<UAnimInstance>> meshLinkEntry;

		//Unlink All
		{
			for (auto AnimClass : _linkedAnimClasses)
			{
				if (AnimClass.Key == nullptr)
					continue;
				for (auto Pair : AnimClass.Value)
				{
					if (Pair.Value == nullptr)
						continue;
					auto instance = Pair.Value;
					if (instance == nullptr)
						continue;
				}
			}
			skeletalMeshReference->LinkAnimClassLayers(nullptr);
		}

		//link
		skeletalMeshReference->LinkAnimClassLayers(animClass);
		if (DebugType == EControllerDebugType::AnimationDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Animation Linking: linked %s to new %s"), *animClass->GetName(), *skeletalMeshReference->GetName()), true, false);
		}

		//Register
		meshLinkEntry.Add(key, skeletalMeshReference->GetLinkedAnimLayerInstanceByClass(animClass));
		_linkedAnimClasses.Add(skeletalMeshReference, meshLinkEntry);
		skeletalMeshReference->SetWorldRotation(lookDir);
		return;
	}

	//The mesh links with a new key
	if (!_linkedAnimClasses[skeletalMeshReference].Contains(key))
	{
		TMap<FName, TWeakObjectPtr<UAnimInstance>> meshLinkEntry;

		//Unlink All
		{
			for (auto AnimClass : _linkedAnimClasses)
			{
				if (AnimClass.Key == nullptr)
					continue;
				for (auto Pair : AnimClass.Value)
				{
					if (Pair.Value == nullptr)
						continue;
					auto instance = Pair.Value;
					if (instance == nullptr)
						continue;
				}
			}
			skeletalMeshReference->LinkAnimClassLayers(nullptr);
		}

		//link
		skeletalMeshReference->LinkAnimClassLayers(animClass);
		if (DebugType == EControllerDebugType::AnimationDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Animation Linking: linked new %s to %s"), *animClass->GetName(), *skeletalMeshReference->GetName()), true, false);
		}

		//Register
		meshLinkEntry.Add(key, skeletalMeshReference->GetLinkedAnimLayerInstanceByClass(animClass));
		_linkedAnimClasses[skeletalMeshReference] = meshLinkEntry;
		skeletalMeshReference->SetWorldRotation(lookDir);
		return;
	}

	if (_linkedAnimClasses[skeletalMeshReference][key] != nullptr)
	{
		if (_linkedAnimClasses[skeletalMeshReference][key]->GetClass() == animClass)
			return;
		//Unlink
		skeletalMeshReference->UnlinkAnimClassLayers(_linkedAnimClasses[skeletalMeshReference][key]->GetClass());
	}
	if (animClass != nullptr)
	{
		//link
		skeletalMeshReference->LinkAnimClassLayers(animClass);
		_linkedAnimClasses[skeletalMeshReference][key] = skeletalMeshReference->GetLinkedAnimLayerInstanceByClass(animClass);
		if (DebugType == EControllerDebugType::AnimationDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Animation Linking: linked %s to %s"), *animClass->GetName(), *skeletalMeshReference->GetName()), true, false);
		}
	}

	skeletalMeshReference->SetWorldRotation(lookDir);
}


double UModularControllerComponent::PlayAnimMontageSingle(UAnimInstance* animInstance, FActionMotionMontage montage, float customAnimStartTime
                                                          , bool useMontageEndCallback, FOnMontageEnded endCallBack)
{
	if (animInstance == nullptr)
	{
		return -1;
	}

	if (montage.Montage == nullptr)
	{
		return -1;
	}

	const int sectionsCount = montage.Montage->GetNumSections();
	const float startTime = customAnimStartTime >= 0 ? customAnimStartTime : 0;
	float duration = animInstance->Montage_Play(montage.Montage, 1, EMontagePlayReturnType::Duration, startTime);
	duration = 0;
	for (int i = 0; i < sectionsCount; i++)
	{
		duration += montage.Montage->GetSectionLength(i);
		if (!montage.Montage->GetAnimCompositeSection(i).NextSectionName.IsValid() || montage.Montage->GetAnimCompositeSection(i).NextSectionName == montage.Montage->GetSectionName(i))
			break;
	}

	if (useMontageEndCallback)
	{
		animInstance->Montage_SetEndDelegate(endCallBack, montage.Montage);
	}

	if (duration <= 0)
	{
		return -1;
	}

	if (!montage.MontageSection.IsNone())
	{
		//Jumps to a section
		animInstance->Montage_JumpToSection(montage.MontageSection, montage.Montage);
		const float newMontagePos = animInstance->Montage_GetPosition(montage.Montage);
		const auto sectionID = montage.Montage->GetSectionIndex(montage.MontageSection);
		duration = 0;
		for (int i = sectionID; i < sectionsCount; i++)
		{
			duration += montage.Montage->GetSectionLength(i);
			if (!montage.Montage->GetAnimCompositeSection(i).NextSectionName.IsValid() || montage.Montage->GetAnimCompositeSection(i).NextSectionName == montage.Montage->GetSectionName(i))
				break;
		}
	}

	duration /= montage.Montage->RateScale;
	return duration;
}


void UModularControllerComponent::OnActionMontageEnds(UAnimMontage* Montage, bool Interrupted)
{
	if (!Montage)
		return;
	if (!_montageOnActionBound.Contains(Montage))
		return;
	if (_montageOnActionBound[Montage].Num() <= 0)
		return;
	for (int i = 0; i < _montageOnActionBound[Montage].Num(); i++)
	{
		if (!_montageOnActionBound[Montage][i].IsValid())
			continue;
		if (!ActionInfos.Contains(_montageOnActionBound[Montage][i]))
			continue;
		auto infos = ActionInfos[_montageOnActionBound[Montage][i]];
		if (infos.GetRemainingActivationTime() <= 0)
			continue;
		ActionInfos[_montageOnActionBound[Montage][i]].SkipTimeToPhase(EActionPhase::Undetermined);
	}
	_montageOnActionBound[Montage].Empty();
	_montageOnActionBound.Remove(Montage);
}


void UModularControllerComponent::ReadRootMotion(FKinematicComponents& kinematics, const FVector fallbackVelocity, const ERootMotionType rootMotionMode, float surfaceFriction,
                                                 float weight) const
{
	if (rootMotionMode != ERootMotionType::NoRootMotion)
	{
		//Rotation
		const FQuat rotDiff = GetRootMotionQuat();
		FVector axis;
		float angle;
		rotDiff.ToAxisAndAngle(axis, angle);
		angle *= weight;
		kinematics.AngularKinematic.Orientation *= FQuat(axis, angle);
	}

	//Translation
	switch (rootMotionMode)
	{
		case ERootMotionType::NoRootMotion:
			UFunctionLibrary::AddCompositeMovement(kinematics.LinearKinematic, fallbackVelocity, -surfaceFriction, 0);
			break;
		default:
			{
				const FVector translation = GetRootMotionTranslation(rootMotionMode, fallbackVelocity);
				UFunctionLibrary::AddCompositeMovement(kinematics.LinearKinematic, FMath::Lerp(fallbackVelocity, translation, weight), -surfaceFriction, 0);
			}
			break;
	}
}


FVector UModularControllerComponent::GetRootMotionTranslation(const ERootMotionType rootMotionMode, const FVector currentVelocity) const
{
	switch (rootMotionMode)
	{
		case ERootMotionType::Additive:
			{
				return GetRootMotionVector() + currentVelocity;
			}
			break;
		case ERootMotionType::Override:
			{
				return GetRootMotionVector();
			}
			break;
		default:
			break;
	}

	return currentVelocity;
}


void UModularControllerComponent::ExtractRootMotions(float delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("EvaluateRootMotions");
	//Extract Root Motion
	if (GetSkeletalMesh().IsValid())
	{
		const FTransform localRootMotion = GetSkeletalMesh()->ConsumeRootMotion().GetRootMotionTransform();
		const FTransform worldRootMotion = GetSkeletalMesh()->ConvertLocalRootMotionToWorld(localRootMotion);
		_RootMotionParams = FTransform(worldRootMotion.GetRotation(), worldRootMotion.GetTranslation() / delta, FVector::OneVector);
	}
}


FControllerStatus UModularControllerComponent::EvaluateRootMotionOverride(const FControllerStatus inStatus, float inDelta, bool& ignoredCollision)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("EvaluateRootMotionOverride");
	FControllerStatus result = inStatus;

	//Handle Root Motion Override
	FOverrideRootMotionCommand* command = nullptr;
	if (_noCollisionOverrideRootMotionCommand.IsValid())
	{
		command = &_noCollisionOverrideRootMotionCommand;
		ignoredCollision = true;
	}
	else if (_overrideRootMotionCommand.IsValid())
		command = &_overrideRootMotionCommand;

	if (!command)
		return result;

	FTransform MotionWarpTransform;
	if (!command->Update(inDelta, MotionWarpTransform, [this,command]()-> void { RemoveMotionWarp(command->WarpKey); }))
		return result;

	//Rotation
	if (command->OverrideRotationRootMotionMode != ERootMotionType::NoRootMotion)
	{
		//Rotation
		result.Kinematics.AngularKinematic.Orientation *= GetRootMotionQuat();
		if (command->IsMotionWarpingEnabled())
			result.Kinematics.AngularKinematic.Orientation = MotionWarpTransform.GetRotation();
	}

	//Translation
	if (command->OverrideTranslationRootMotionMode != ERootMotionType::NoRootMotion)
	{
		const FVector matchingMove = MotionWarpTransform.GetLocation() - result.Kinematics.LinearKinematic.Position;
		switch (command->OverrideTranslationRootMotionMode)
		{
			case ERootMotionType::Additive:
				{
					result.Kinematics.LinearKinematic.Velocity += GetRootMotionVector();
					if (command->IsMotionWarpingEnabled())
						result.Kinematics.LinearKinematic.Velocity += matchingMove;
				}
				break;
			case ERootMotionType::Override:
				{
					result.Kinematics.LinearKinematic.Velocity = GetRootMotionVector();
					if (command->IsMotionWarpingEnabled())
					{
						UFunctionLibrary::AddCompositeMovement(result.Kinematics.LinearKinematic, matchingMove / inDelta, -1, 0);
						result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
					}
				}
				break;
			default:
				break;
		}
	}

	//Debug
	if (DebugType == EControllerDebugType::AnimationDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Override RM. trMode(%s), rtMode(%s), time(%f/%f), WarpKey(%s), WarpPathCount(%d)")
		                                                        , *UEnum::GetValueAsString(command->OverrideTranslationRootMotionMode)
		                                                        , *UEnum::GetValueAsString(command->OverrideRotationRootMotionMode)
		                                                        , command->Time, command->Duration
		                                                        , *command->WarpKey.ToString()
		                                                        , command->WarpTransform_Path.Num()
		                                  ), true, false, FColor::Red, 0, "RMOverride");
		if (command->WarpTransform_Path.Num() > 1)
		{
			for (int i = 1; i < command->WarpTransform_Path.Num(); i++)
			{
				UKismetSystemLibrary::DrawDebugArrow(this, command->WarpTransform_Path[i - 1].GetLocation(), command->WarpTransform_Path[i].GetLocation()
					, 50, FColor::Red, inDelta * 1.2);
			}
		}
	}

	return result;
}


#pragma endregion
