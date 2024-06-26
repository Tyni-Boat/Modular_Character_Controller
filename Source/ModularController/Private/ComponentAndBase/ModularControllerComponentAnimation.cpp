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
			if (state->StateBlueprintClass == nullptr)
				return nullptr;
			return GetSkeletalMesh()->GetLinkedAnimLayerInstanceByClass(state->StateBlueprintClass);
		}
		return GetSkeletalMesh()->GetAnimInstance();
	}
	return nullptr;
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
		TMap <FName, TWeakObjectPtr<UAnimInstance>> meshLinkEntry;

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
		if(DebugType == ControllerDebugType_AnimationDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Animation Linking: linked %s to new %s"), *animClass->GetName(), *skeletalMeshReference->GetName()));
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
		TMap <FName, TWeakObjectPtr<UAnimInstance>> meshLinkEntry;

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
		if (DebugType == ControllerDebugType_AnimationDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Animation Linking: linked new %s to %s"), *animClass->GetName(), *skeletalMeshReference->GetName()));
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
		if (DebugType == ControllerDebugType_AnimationDebug)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Animation Linking: linked %s to %s"), *animClass->GetName(), *skeletalMeshReference->GetName()));
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

	const float startTime = customAnimStartTime >= 0 ? customAnimStartTime : 0;
	float duration = animInstance->Montage_Play(montage.Montage, 1, EMontagePlayReturnType::Duration, startTime);
	duration = montage.Montage->GetSectionLength(0);

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
		duration = montage.Montage->GetSectionLength(sectionID);
	}

	return duration;
}


void UModularControllerComponent::ReadRootMotion(FKinematicComponents& kinematics, const ERootMotionType rootMotionMode) const
{
	if (rootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
	{
		//Rotation
		kinematics.AngularKinematic.Orientation *= GetRootMotionQuat();

		//Translation
		switch (rootMotionMode)
		{
			case RootMotionType_No_RootMotion:
				break;
			default:
			{
				const FVector translation = GetRootMotionTranslation(rootMotionMode, kinematics.LinearKinematic.Velocity);
				kinematics.LinearKinematic.AddCompositeMovement(translation, -1, 0);
			}
			break;
		}
	}
}


FVector UModularControllerComponent::GetRootMotionTranslation(const ERootMotionType rootMotionMode, const FVector currentVelocity) const
{
	switch (rootMotionMode)
	{
		case RootMotionType_Additive:
		{
			return GetRootMotionVector() + currentVelocity;
		}
		break;
		case RootMotionType_Override:
		{
			return GetRootMotionVector();
		}
		break;
		default:
			break;
	}

	return currentVelocity;
}


void UModularControllerComponent::EvaluateRootMotions(float delta)
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


FControllerStatus UModularControllerComponent::EvaluateRootMotionOverride(const FControllerStatus inStatus, float inDelta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("EvaluateRootMotionOverride");
	FControllerStatus result = inStatus;

	//Handle Root Motion Override
	{
		//Rotation
		if (_overrideRootMotionCommand.OverrideRotationRootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
		{
			//Rotation
			result.Kinematics.AngularKinematic.Orientation *= GetRootMotionQuat();
		}

		//Translation
		if (_overrideRootMotionCommand.OverrideTranslationRootMotionMode != ERootMotionType::RootMotionType_No_RootMotion)
		{
			switch (_overrideRootMotionCommand.OverrideTranslationRootMotionMode)
			{
				case RootMotionType_Additive:
				{
					result.Kinematics.LinearKinematic.Velocity += GetRootMotionVector();
				}break;
				case RootMotionType_Override:
				{
					result.Kinematics.LinearKinematic.Velocity = GetRootMotionVector();
				}break;
			}
		}

		//Auto restore
		if (_overrideRootMotionCommand.OverrideRootMotionChrono > 0)
		{
			_overrideRootMotionCommand.OverrideRootMotionChrono -= inDelta;
			if (_overrideRootMotionCommand.OverrideRootMotionChrono <= 0)
			{
				_overrideRootMotionCommand.OverrideRootMotionChrono = ERootMotionType::RootMotionType_No_RootMotion;
				_overrideRootMotionCommand.OverrideRootMotionChrono = ERootMotionType::RootMotionType_No_RootMotion;
			}
		}
	}

	return result;
}


#pragma endregion
