// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include <functional>
#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"


#pragma region All Behaviours


void UModularControllerComponent::SetOverrideRootMotion(const FOverrideRootMotionCommand rootMotionParams, bool IgnoreCollision)
{
	if (IgnoreCollision)
	{
		_noCollisionOverrideRootMotionCommand = rootMotionParams;
	}
	else
	{
		_overrideRootMotionCommand = rootMotionParams;
	}
}

#pragma endregion


#pragma region States


UBaseControllerState* UModularControllerComponent::GetCurrentControllerState() const
{
	if (StatesInstances.IsValidIndex(ComputedControllerStatus.StatusParams.StateIndex))
		return StatesInstances[ComputedControllerStatus.StatusParams.StateIndex].Get();
	return nullptr;
}

bool UModularControllerComponent::CheckControllerStateByType(TSubclassOf<UBaseControllerState> moduleType) const
{
	if (StatesInstances.Num() <= 0)
		return false;
	const auto index = StatesInstances.IndexOfByPredicate([moduleType](const TSoftObjectPtr<UBaseControllerState>& state)
	{
		return state.IsValid() && state->GetClass() == moduleType;
	});
	return StatesInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckControllerStateByName(FName moduleName) const
{
	if (StatesInstances.Num() <= 0)
		return false;
	const auto index = StatesInstances.IndexOfByPredicate([moduleName](const TSoftObjectPtr<UBaseControllerState>& state)
	{
		return state.IsValid() && state->GetDescriptionName() == moduleName;
	});
	return StatesInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckControllerStateByPriority(int modulePriority) const
{
	if (StatesInstances.Num() <= 0)
		return false;
	const auto index = StatesInstances.IndexOfByPredicate([modulePriority](const TSoftObjectPtr<UBaseControllerState>& state)
	{
		return state.IsValid() && state->GetPriority() == modulePriority;
	});
	return StatesInstances.IsValidIndex(index);
}

void UModularControllerComponent::SortStates()
{
	if (StatesInstances.Num() > 1)
	{
		StatesInstances.Sort([](const TSoftObjectPtr<UBaseControllerState>& a, const TSoftObjectPtr<UBaseControllerState>& b)
		{
			return a.IsValid() && b.IsValid() && a->GetPriority() > b->GetPriority();
		});
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::AddControllerState_Implementation(TSubclassOf<UBaseControllerState> moduleType)
{
	if (!moduleType)
		return;
	if (CheckControllerStateByType(moduleType.Get()))
		return;
	StatesInstances.Add(moduleType->GetDefaultObject());
	SortStates();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UBaseControllerState* UModularControllerComponent::GetControllerStateByType(TSubclassOf<UBaseControllerState> moduleType)
{
	if (StatesInstances.Num() <= 0)
		return nullptr;
	const auto index = StatesInstances.IndexOfByPredicate([moduleType](const TSoftObjectPtr<UBaseControllerState>& state) { return state.IsValid() && state->GetClass() == moduleType; });
	if (StatesInstances.IsValidIndex(index))
	{
		return StatesInstances[index].Get();
	}
	return nullptr;
}

UBaseControllerState* UModularControllerComponent::GetControllerStateByName(FName moduleName)
{
	if (StatesInstances.Num() <= 0)
		return nullptr;
	const auto index = StatesInstances.IndexOfByPredicate([moduleName](const TSoftObjectPtr<UBaseControllerState>& state)
	{
		return state.IsValid() && state->GetDescriptionName() == moduleName;
	});
	if (StatesInstances.IsValidIndex(index))
		return StatesInstances[index].Get();
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::RemoveControllerStateByType_Implementation(TSubclassOf<UBaseControllerState> moduleType)
{
	if (CheckControllerStateByType(moduleType))
	{
		const auto state = StatesInstances.FindByPredicate([moduleType](const TSoftObjectPtr<UBaseControllerState>& st)
		{
			return st.IsValid() && st->GetClass() == moduleType->GetClass();
		});
		StatesInstances.Remove(*state);
		SortStates();
	}
}

void UModularControllerComponent::RemoveControllerStateByName_Implementation(FName moduleName)
{
	if (CheckControllerStateByName(moduleName))
	{
		auto state = StatesInstances.FindByPredicate([moduleName](const TSoftObjectPtr<UBaseControllerState>& st)
		{
			return st.IsValid() && st->GetDescriptionName() == moduleName;
		});
		StatesInstances.Remove(*state);
		SortStates();
	}
}

void UModularControllerComponent::RemoveControllerStateByPriority_Implementation(int modulePriority)
{
	if (CheckControllerStateByPriority(modulePriority))
	{
		auto state = StatesInstances.FindByPredicate([modulePriority](const TSoftObjectPtr<UBaseControllerState>& st)
		{
			return st.IsValid() && st->GetPriority() == modulePriority;
		});
		StatesInstances.Remove(*state);
		SortStates();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FControllerStatus UModularControllerComponent::CheckControllerStates(FControllerStatus currentControllerStatus, const float inDelta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CheckControllerStates");
	FControllerStatus endStatus = currentControllerStatus;
	FControllerStatus selectedStatus = endStatus;
	int selectedStateIndex = -1;

	//Check if a State's check have success state
	{
		int maxStatePriority = -1;
		bool overrideNewState = false;

		//Check if a valid action freeze the current state
		if (selectedStateIndex < 0)
		{
			const int activeActionIndex = endStatus.StatusParams.ActionIndex;

			if (ActionInstances.IsValidIndex(activeActionIndex) && ActionInstances[activeActionIndex].IsValid())
			{
				//Find state freeze
				if (ActionInstances[activeActionIndex]->bFreezeCurrentState)
				{
					selectedStateIndex = endStatus.StatusParams.StateIndex;
				}

				//Find last frame status voider
				if (ActionInstances[activeActionIndex]->bShouldControllerStateCheckOverride)
				{
					overrideNewState = true;
				}
			}
		}

		if (selectedStateIndex < 0)
		{
			if (CurrentPerformanceProfile.bUseMultiThreadState)
			{
				TQueue<TTuple<int, FControllerCheckResult>> res = TQueue<TTuple<int, FControllerCheckResult>>();
				ParallelFor(StatesInstances.Num(), [&](int32 i)
				{
					if (StatesInstances.IsValidIndex(i) && StatesInstances[i].IsValid())
					{
						const auto checkResult = StatesInstances[i]->CheckState(this, endStatus, inDelta, overrideNewState ? false : endStatus.StatusParams.StateIndex == i);
						res.Enqueue(TTuple<int, FControllerCheckResult>(i, checkResult));
					}
				});
				TTuple<int, FControllerCheckResult> item;
				while (res.Dequeue(item))
				{
					endStatus.StatusParams.AppendCosmetics(item.Value.ProcessResult.StatusParams.StatusCosmeticVariables);

					//Select the highest priority only
					if (StatesInstances[item.Key]->GetPriority() < maxStatePriority)
						continue;

					if (item.Value.CheckedCondition)
					{
						selectedStateIndex = item.Key;
						selectedStatus = item.Value.ProcessResult;
						maxStatePriority = StatesInstances[item.Key]->GetPriority();
					}
				}
			}
			else
			{
				for (int i = 0; i < StatesInstances.Num(); i++)
				{
					if (!StatesInstances[i].IsValid())
						continue;

					const auto checkResult = StatesInstances[i]->CheckState(this, endStatus, inDelta, overrideNewState ? false : endStatus.StatusParams.StateIndex == i);
					endStatus.StatusParams.AppendCosmetics(checkResult.ProcessResult.StatusParams.StatusCosmeticVariables);

					//Select the highest priority only
					if (StatesInstances[i]->GetPriority() < maxStatePriority)
						continue;

					if (checkResult.CheckedCondition)
					{
						selectedStateIndex = i;
						selectedStatus = checkResult.ProcessResult;
						maxStatePriority = StatesInstances[i]->GetPriority();
					}
				}
			}
		}
	}

	selectedStatus.StatusParams.StatusCosmeticVariables = endStatus.StatusParams.StatusCosmeticVariables;
	endStatus = selectedStatus;
	endStatus.StatusParams.StateIndex = selectedStateIndex;
	return endStatus;
}


FControllerStatus UModularControllerComponent::CosmeticCheckState(FControllerStatus currentControllerStatus, const float inDelta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CosmeticCheckState");
	FControllerStatus endStatus = currentControllerStatus;

	//Check if a State's check have success state
	{
		if (CurrentPerformanceProfile.bUseMultiThreadState)
		{
			TQueue<TTuple<int, FControllerCheckResult>> res = TQueue<TTuple<int, FControllerCheckResult>>();
			ParallelFor(StatesInstances.Num(), [&](int32 i)
			{
				if (StatesInstances.IsValidIndex(i) && StatesInstances[i].IsValid())
				{
					const auto checkResult = StatesInstances[i]->CheckState(this, endStatus, inDelta, currentControllerStatus.StatusParams.StateIndex == i);
					res.Enqueue(TTuple<int, FControllerCheckResult>(i, checkResult));
				}
			});
			TTuple<int, FControllerCheckResult> item;
			while (res.Dequeue(item))
			{
				endStatus.StatusParams.AppendCosmetics(item.Value.ProcessResult.StatusParams.StatusCosmeticVariables);
				if (currentControllerStatus.StatusParams.StateIndex == item.Key)
				{
					endStatus = item.Value.ProcessResult;
				}
			}
		}
		else
		{
			for (int i = 0; i < StatesInstances.Num(); i++)
			{
				if (!StatesInstances[i].IsValid())
					continue;

				const auto checkResult = StatesInstances[i]->CheckState(this, endStatus, inDelta, currentControllerStatus.StatusParams.StateIndex == i);
				endStatus.StatusParams.AppendCosmetics(checkResult.ProcessResult.StatusParams.StatusCosmeticVariables);
				if (currentControllerStatus.StatusParams.StateIndex == i)
				{
					endStatus = checkResult.ProcessResult;
				}
			}
		}
	}

	return endStatus;
}


FControllerCheckResult UModularControllerComponent::TryChangeControllerState(FControllerStatus ToStateStatus, FControllerStatus fromStateStatus) const
{
	int fromIndex = fromStateStatus.StatusParams.StateIndex;
	int toIndex = ToStateStatus.StatusParams.StateIndex;
	FControllerCheckResult result = FControllerCheckResult(false, fromStateStatus);
	result.ProcessResult.StatusParams.StatusCosmeticVariables = ToStateStatus.StatusParams.StatusCosmeticVariables;

	if (!StatesInstances.IsValidIndex(toIndex))
		return result;

	if (toIndex == fromIndex)
	{
		result.ProcessResult.Kinematics = ToStateStatus.Kinematics;
		return result;
	}

	ToStateStatus.StatusParams.StateModifiers = FVector(0);
	result = FControllerCheckResult(true, ToStateStatus);
	return result;
}


void UModularControllerComponent::ChangeControllerState(FControllerStatus ToStateStatus, const float inDelta)
{
	const int fromIndex = ApplyedControllerStatus.StatusParams.StateIndex;
	const int toIndex = ToStateStatus.StatusParams.StateIndex;

	if (toIndex == fromIndex)
	{
		return;
	}

	if (!StatesInstances.IsValidIndex(toIndex) || !StatesInstances[toIndex].IsValid())
	{
		return;
	}

	if (StatesInstances.IsValidIndex(fromIndex) && StatesInstances[fromIndex].IsValid())
	{
		//Leaving
		StatesInstances[fromIndex]->OnExitState(this, ToStateStatus.Kinematics, ToStateStatus.MoveInput, inDelta);
	}

	//Landing
	StatesInstances[toIndex]->OnEnterState(this, ToStateStatus.Kinematics, ToStateStatus.MoveInput, inDelta);
	const auto linkClass = StatesOverrideAnimInstances.Contains(StatesInstances[toIndex]->GetDescriptionName())
		                       ? StatesOverrideAnimInstances[StatesInstances[toIndex]->GetDescriptionName()]
		                       : StatesInstances[toIndex]->StateFallbackBlueprintClass;
	LinkAnimBlueprint(GetSkeletalMesh(), "State", linkClass);

	//Reset the time spend on state
	TimeOnCurrentState = 0;

	//Notify the controller
	OnControllerStateChanged(StatesInstances[toIndex].Get(), StatesInstances.IsValidIndex(fromIndex) ? StatesInstances[fromIndex].Get() : nullptr);
	OnControllerStateChangedEvent.Broadcast(StatesInstances[toIndex].Get(), StatesInstances.IsValidIndex(fromIndex) ? StatesInstances[fromIndex].Get() : nullptr);
}


FControllerStatus UModularControllerComponent::ProcessControllerState(const FControllerStatus initialState, const float inDelta)
{
	FControllerStatus processMotion = initialState;
	const int index = initialState.StatusParams.StateIndex;

	if (StatesInstances.IsValidIndex(index) && StatesInstances[index].IsValid())
	{
		TimeOnCurrentState += inDelta;
		processMotion = StatesInstances[index]->ProcessState(this, initialState, inDelta);

		if (DebugType == EControllerDebugType::StatusDebug)
		{
			UKismetSystemLibrary::PrintString(
				GetWorld(), FString::Printf(TEXT("State (%s) is Being Processed. Index: %d. Time In: %f"), *StatesInstances[index]->DebugString(), index, TimeOnCurrentState), true, false,
				FColor::White,
				5
				, "ProcessControllerState");
		}
	}

	return processMotion;
}


void UModularControllerComponent::OnControllerStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState)
{
}


#pragma endregion


#pragma region Actions


UBaseControllerAction* UModularControllerComponent::GetCurrentControllerAction() const
{
	if (ActionInstances.IsValidIndex(ComputedControllerStatus.StatusParams.ActionIndex))
	{
		return ActionInstances[ComputedControllerStatus.StatusParams.ActionIndex].Get();
	}
	return nullptr;
}

FActionInfos UModularControllerComponent::GetCurrentControllerActionInfos() const
{
	const auto curAction = GetCurrentControllerAction();
	if (!curAction)
		return FActionInfos();
	return ActionInfos.Contains(curAction) ? ActionInfos[curAction] : FActionInfos();
}

bool UModularControllerComponent::IsActionMontagePlaying() const
{
	const auto curAction = GetCurrentControllerAction();
	if (!curAction)
		return false;
	if (curAction->GetDescriptionName() == "BuiltIn_ActionMontage")
		return true;
	return false;
}


bool UModularControllerComponent::CheckActionBehaviourByType(TSubclassOf<UBaseControllerAction> moduleType) const
{
	if (ActionInstances.Num() <= 0)
		return false;
	const auto index = ActionInstances.IndexOfByPredicate([moduleType](const TSoftObjectPtr<UBaseControllerAction>& action)
	{
		return action.IsValid() && action->GetClass() == moduleType;
	});
	return ActionInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckActionBehaviourByName(FName moduleName) const
{
	if (ActionInstances.Num() <= 0)
		return false;
	const auto index = ActionInstances.IndexOfByPredicate([moduleName](const TSoftObjectPtr<UBaseControllerAction>& action)
	{
		return action.IsValid() && action->GetDescriptionName() == moduleName;
	});
	return ActionInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckActionBehaviourByPriority(int modulePriority) const
{
	if (ActionInstances.Num() <= 0)
		return false;
	const auto index = ActionInstances.IndexOfByPredicate([modulePriority](const TSoftObjectPtr<UBaseControllerAction>& action)
	{
		return action.IsValid() && action->GetPriority() == modulePriority;
	});
	return ActionInstances.IsValidIndex(index);
}

void UModularControllerComponent::SortActions()
{
	if (ActionInstances.Num() > 1)
	{
		//Save action montage
		const auto actionMontage = ActionInstances[0];
		ActionInstances.RemoveAt(0);
		if (ActionInstances.Num() > 1)
		{
			ActionInstances.Sort([](const TSoftObjectPtr<UBaseControllerAction>& a, const TSoftObjectPtr<UBaseControllerAction>& b)
			{
				return a.IsValid() && b.IsValid() && a->GetPriority() > b->GetPriority();
			});
		}
		ActionInstances.Insert(actionMontage, 0);
	}

	//Remove null references
	ActionInfos = ActionInfos.FilterByPredicate([](TTuple<TSoftObjectPtr<UBaseControllerAction>, FActionInfos> item) -> bool
	{
		return item.Key.IsValid();
	});

	//Add new references
	for (int i = 0; i < ActionInstances.Num(); i++)
	{
		if (!ActionInstances[i].IsValid())
			continue;
		if (ActionInfos.Contains(ActionInstances[i]))
			continue;
		ActionInfos.Add(ActionInstances[i], FActionInfos());
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::AddControllerAction_Implementation(TSubclassOf<UBaseControllerAction> moduleType)
{
	if (!moduleType)
		return;
	if (CheckActionBehaviourByType(moduleType.Get()))
		return;
	ActionInstances.Add(moduleType->GetDefaultObject());
	SortActions();
}


UBaseControllerAction* UModularControllerComponent::GetActionByType(TSubclassOf<UBaseControllerAction> moduleType)
{
	if (ActionInstances.Num() <= 0)
		return nullptr;
	const auto index = ActionInstances.IndexOfByPredicate([moduleType](const TSoftObjectPtr<UBaseControllerAction>& action)
	{
		return action.IsValid() && action->GetClass() == moduleType;
	});
	if (ActionInstances.IsValidIndex(index))
	{
		return ActionInstances[index].Get();
	}
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UModularControllerComponent::RemoveActionBehaviourByType_Implementation(TSubclassOf<UBaseControllerAction> moduleType)
{
	if (CheckActionBehaviourByType(moduleType))
	{
		auto behaviour = ActionInstances.FindByPredicate([moduleType](const TSoftObjectPtr<UBaseControllerAction>& action)
		{
			return action.IsValid() && action->GetClass() == moduleType->GetClass();
		});
		ActionInstances.Remove(*behaviour);
		SortActions();
	}
}

void UModularControllerComponent::RemoveActionBehaviourByName_Implementation(FName moduleName)
{
	if (CheckActionBehaviourByName(moduleName))
	{
		auto behaviour = ActionInstances.FindByPredicate([moduleName](const TSoftObjectPtr<UBaseControllerAction>& action)
		{
			return action.IsValid() && action->GetDescriptionName() == moduleName;
		});
		ActionInstances.Remove(*behaviour);
		SortActions();
	}
}

void UModularControllerComponent::RemoveActionBehaviourByPriority_Implementation(int modulePriority)
{
	if (CheckActionBehaviourByPriority(modulePriority))
	{
		auto behaviour = ActionInstances.FindByPredicate([modulePriority](TSoftObjectPtr<UBaseControllerAction>& action)
		{
			return action.IsValid() && action->GetPriority() == modulePriority;
		});
		ActionInstances.Remove(*behaviour);
		SortActions();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FActionMotionMontage UModularControllerComponent::GetActionCurrentMotionMontage(const UBaseControllerAction* actionInst) const
{
	if (!actionInst)
		return FActionMotionMontage();
	FActionMontageLibrary actionMontageLibrary = ActionMontageLibraryMap.Contains(actionInst->GetDescriptionName())
		                                             ? ActionMontageLibraryMap[actionInst->GetDescriptionName()]
		                                             : FActionMontageLibrary();
	if (!ActionInfos.Contains(actionInst))
		return FActionMotionMontage();

	if (!actionMontageLibrary.Library.IsValidIndex(ActionInfos[actionInst]._montageLibraryIndex))
		return FActionMotionMontage();

	return actionMontageLibrary.Library[ActionInfos[actionInst]._montageLibraryIndex];
}


FControllerStatus UModularControllerComponent::CheckControllerActions(FControllerStatus currentControllerStatus, const float inDelta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CheckControllerActions");
	FControllerStatus endStatus = currentControllerStatus;
	FControllerStatus selectedStatus = endStatus;
	int selectedActionIndex = -1;

	//Check active action still active
	selectedActionIndex = endStatus.StatusParams.ActionIndex;
	if (ActionInstances.IsValidIndex(selectedActionIndex) && ActionInstances[selectedActionIndex].IsValid() && ActionInfos.Contains(ActionInstances[selectedActionIndex]))
	{
		if (ActionInfos[ActionInstances[selectedActionIndex]].CurrentPhase == EActionPhase::Recovery
			&& ActionInstances[selectedActionIndex]->bCanTransitionToSelf
			&& CheckActionCompatibility(ActionInstances[selectedActionIndex], endStatus.StatusParams.StateIndex, endStatus.StatusParams.ActionIndex))
		{
			const auto chkResult = ActionInstances[selectedActionIndex]->CheckAction(this, endStatus, inDelta, true);
			if (chkResult.CheckedCondition)
			{
				selectedStatus = chkResult.ProcessResult;
				selectedStatus.StatusParams.PrimaryActionFlag = 1;
			}
		}

		if (ActionInfos[ActionInstances[selectedActionIndex]].GetRemainingActivationTime() <= 0)
		{
			selectedActionIndex = -1;
			selectedStatus = endStatus;
		}
	}

	//Check actions
	{
		if (CurrentPerformanceProfile.bUseMultiThreadAction)
		{
			TQueue<TTuple<int, FControllerCheckResult>> res = TQueue<TTuple<int, FControllerCheckResult>>();
			ParallelFor(StatesInstances.Num(), [&](int32 i)
			{
				if (selectedActionIndex == i)
					return;

				if (!ActionInstances.IsValidIndex(i))
					return;
				if (!ActionInstances[i].IsValid())
					return;
				if (!ActionInfos.Contains(ActionInstances[i]))
					return;

				const auto currentPhase = ActionInfos[ActionInstances[i]].CurrentPhase;
				if (currentPhase == EActionPhase::Anticipation || currentPhase == EActionPhase::Active)
					return;
				if (currentPhase == EActionPhase::Recovery && !ActionInstances[i]->bCanTransitionToSelf)
					return;
				if (ActionInfos[ActionInstances[i]].GetRemainingCoolDownTime() > 0 && !ActionInstances[i]->bCanTransitionToSelf)
					return;

				if (CheckActionCompatibility(ActionInstances[i], endStatus.StatusParams.StateIndex, endStatus.StatusParams.ActionIndex))
				{
					const auto chkResult = ActionInstances[i]->CheckAction(this, endStatus, inDelta, i == endStatus.StatusParams.ActionIndex);
					res.Enqueue(TTuple<int, FControllerCheckResult>(i, chkResult));
				}
			});
			TTuple<int, FControllerCheckResult> item;
			while (res.Dequeue(item))
			{
				if (ActionInstances.IsValidIndex(selectedActionIndex) && ActionInstances[selectedActionIndex].IsValid())
				{
					if (ActionInstances[item.Key]->GetPriority() <= ActionInstances[selectedActionIndex]->GetPriority())
					{
						if (ActionInstances[item.Key]->GetPriority() != ActionInstances[selectedActionIndex]->GetPriority())
							continue;
						if (ActionInstances[item.Key]->GetPriority() == ActionInstances[selectedActionIndex]->GetPriority()
							&& ActionInfos.Contains(ActionInstances[selectedActionIndex])
							&& ActionInfos[ActionInstances[selectedActionIndex]].CurrentPhase != EActionPhase::Recovery)
						{
							continue;
						}
					}
				}

				endStatus.StatusParams.AppendCosmetics(item.Value.ProcessResult.StatusParams.StatusCosmeticVariables);
				if (item.Value.CheckedCondition)
				{
					selectedActionIndex = item.Key;
					selectedStatus = item.Value.ProcessResult;

					if (DebugType == EControllerDebugType::StatusDebug)
					{
						UKismetSystemLibrary::PrintString(
							GetWorld(), FString::Printf(TEXT("Action (%s) was checked as active. Remaining Time: %f"), *ActionInstances[item.Key]->DebugString(),
							                            ActionInfos[ActionInstances[item.Key]].GetRemainingActivationTime()), true, false, FColor::Silver, 0
							, FName(FString::Printf(TEXT("CheckControllerActions_%s"), *ActionInstances[item.Key]->GetDescriptionName().ToString())));
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < ActionInstances.Num(); i++)
			{
				if (selectedActionIndex == i)
					continue;

				if (!ActionInstances[i].IsValid())
					continue;
				if (!ActionInfos.Contains(ActionInstances[i]))
					continue;

				if (ActionInstances.IsValidIndex(selectedActionIndex) && ActionInstances[selectedActionIndex].IsValid())
				{
					if (ActionInstances[i]->GetPriority() <= ActionInstances[selectedActionIndex]->GetPriority())
					{
						if (ActionInstances[i]->GetPriority() != ActionInstances[selectedActionIndex]->GetPriority())
							continue;
						if (ActionInstances[i]->GetPriority() == ActionInstances[selectedActionIndex]->GetPriority()
							&& ActionInfos.Contains(ActionInstances[selectedActionIndex])
							&& ActionInfos[ActionInstances[selectedActionIndex]].CurrentPhase != EActionPhase::Recovery)
						{
							continue;
						}
					}
				}

				const auto currentPhase = ActionInfos[ActionInstances[i]].CurrentPhase;
				if (currentPhase == EActionPhase::Anticipation || currentPhase == EActionPhase::Active)
					continue;
				if (currentPhase == EActionPhase::Recovery && !ActionInstances[i]->bCanTransitionToSelf)
					continue;
				if (ActionInfos[ActionInstances[i]].GetRemainingCoolDownTime() > 0 && !ActionInstances[i]->bCanTransitionToSelf)
					continue;

				if (CheckActionCompatibility(ActionInstances[i], endStatus.StatusParams.StateIndex, endStatus.StatusParams.ActionIndex))
				{
					const auto chkResult = ActionInstances[i]->CheckAction(this, endStatus, inDelta, i == endStatus.StatusParams.ActionIndex);
					endStatus.StatusParams.AppendCosmetics(chkResult.ProcessResult.StatusParams.StatusCosmeticVariables);
					if (chkResult.CheckedCondition)
					{
						selectedActionIndex = i;
						selectedStatus = chkResult.ProcessResult;

						if (DebugType == EControllerDebugType::StatusDebug)
						{
							UKismetSystemLibrary::PrintString(
								GetWorld(), FString::Printf(TEXT("Action (%s) was checked as active. Remaining Time: %f"), *ActionInstances[i]->DebugString(),
								                            ActionInfos[ActionInstances[i]].GetRemainingActivationTime()), true, false, FColor::Silver, 0
								, FName(FString::Printf(TEXT("CheckControllerActions_%s"), *ActionInstances[i]->GetDescriptionName().ToString())));
						}
					}
				}
			}
		}
	}

	if (DebugType == EControllerDebugType::StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Check Action: Selected Index (%d)"), selectedActionIndex), true, false, FColor::Silver, 0
		                                  , TEXT("CheckControllerActions"));
	}

	selectedStatus.StatusParams.StatusCosmeticVariables = endStatus.StatusParams.StatusCosmeticVariables;
	endStatus = selectedStatus;
	endStatus.StatusParams.ActionIndex = selectedActionIndex;
	return endStatus;
}


FControllerStatus UModularControllerComponent::CosmeticCheckActions(FControllerStatus currentControllerStatus, const float inDelta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CosmeticCheckActions");
	FControllerStatus endStatus = currentControllerStatus;
	int selectedActionIndex = endStatus.StatusParams.ActionIndex;

	//Check active action still active
	if (ActionInstances.IsValidIndex(selectedActionIndex) && ActionInstances[selectedActionIndex].IsValid() && ActionInfos.Contains(ActionInstances[selectedActionIndex]))
	{
		const auto chkResult = ActionInstances[selectedActionIndex]->CheckAction(this, endStatus, inDelta, true);
		endStatus = chkResult.ProcessResult;
		endStatus.StatusParams.StatusCosmeticVariables = chkResult.ProcessResult.StatusParams.StatusCosmeticVariables;
	}

	//Check actions
	{
		if (CurrentPerformanceProfile.bUseMultiThreadAction)
		{
			TQueue<TTuple<int, FControllerCheckResult>> res = TQueue<TTuple<int, FControllerCheckResult>>();
			ParallelFor(StatesInstances.Num(), [&](int32 i)
			{
				if (selectedActionIndex == i)
					return;

				if (!ActionInstances.IsValidIndex(i))
					return;
				if (!ActionInstances[i].IsValid())
					return;
				if (!ActionInfos.Contains(ActionInstances[i]))
					return;

				const auto currentPhase = ActionInfos[ActionInstances[i]].CurrentPhase;
				if (currentPhase == EActionPhase::Anticipation || currentPhase == EActionPhase::Active)
					return;
				if (currentPhase == EActionPhase::Recovery && !ActionInstances[i]->bCanTransitionToSelf)
					return;
				if (ActionInfos[ActionInstances[i]].GetRemainingCoolDownTime() > 0 && !ActionInstances[i]->bCanTransitionToSelf)
					return;

				if (CheckActionCompatibility(ActionInstances[i], endStatus.StatusParams.StateIndex, endStatus.StatusParams.ActionIndex))
				{
					const auto chkResult = ActionInstances[i]->CheckAction(this, endStatus, inDelta, i == endStatus.StatusParams.ActionIndex);
					res.Enqueue(TTuple<int, FControllerCheckResult>(i, chkResult));
				}
			});
			TTuple<int, FControllerCheckResult> item;
			while (res.Dequeue(item))
			{
				if (ActionInstances.IsValidIndex(selectedActionIndex) && ActionInstances[selectedActionIndex].IsValid())
				{
					if (ActionInstances[item.Key]->GetPriority() <= ActionInstances[selectedActionIndex]->GetPriority())
					{
						if (ActionInstances[item.Key]->GetPriority() != ActionInstances[selectedActionIndex]->GetPriority())
							continue;
						if (ActionInstances[item.Key]->GetPriority() == ActionInstances[selectedActionIndex]->GetPriority()
							&& ActionInfos.Contains(ActionInstances[selectedActionIndex])
							&& ActionInfos[ActionInstances[selectedActionIndex]].CurrentPhase != EActionPhase::Recovery)
						{
							continue;
						}
					}
				}
				endStatus.StatusParams.AppendCosmetics(item.Value.ProcessResult.StatusParams.StatusCosmeticVariables);
			}
		}
		else
		{
			for (int i = 0; i < ActionInstances.Num(); i++)
			{
				if (selectedActionIndex == i)
					continue;

				if (!ActionInstances[i].IsValid())
					continue;
				if (!ActionInfos.Contains(ActionInstances[i]))
					continue;

				if (ActionInstances.IsValidIndex(selectedActionIndex) && ActionInstances[selectedActionIndex].IsValid())
				{
					if (ActionInstances[i]->GetPriority() <= ActionInstances[selectedActionIndex]->GetPriority())
					{
						if (ActionInstances[i]->GetPriority() != ActionInstances[selectedActionIndex]->GetPriority())
							continue;
						if (ActionInstances[i]->GetPriority() == ActionInstances[selectedActionIndex]->GetPriority()
							&& ActionInfos.Contains(ActionInstances[selectedActionIndex])
							&& ActionInfos[ActionInstances[selectedActionIndex]].CurrentPhase != EActionPhase::Recovery)
						{
							continue;
						}
					}
				}

				const auto currentPhase = ActionInfos[ActionInstances[i]].CurrentPhase;
				if (currentPhase == EActionPhase::Anticipation || currentPhase == EActionPhase::Active)
					continue;
				if (currentPhase == EActionPhase::Recovery && !ActionInstances[i]->bCanTransitionToSelf)
					continue;
				if (ActionInfos[ActionInstances[i]].GetRemainingCoolDownTime() > 0 && !ActionInstances[i]->bCanTransitionToSelf)
					continue;

				if (CheckActionCompatibility(ActionInstances[i], endStatus.StatusParams.StateIndex, endStatus.StatusParams.ActionIndex))
				{
					const auto chkResult = ActionInstances[i]->CheckAction(this, endStatus, inDelta, i == endStatus.StatusParams.ActionIndex);
					endStatus.StatusParams.StatusCosmeticVariables = chkResult.ProcessResult.StatusParams.StatusCosmeticVariables;
				}
			}
		}
	}

	return endStatus;
}


bool UModularControllerComponent::CheckActionCompatibility(const TSoftObjectPtr<UBaseControllerAction> actionInstance, int stateIndex, int actionIndex) const
{
	if (!actionInstance.IsValid())
		return false;

	bool incompatible = false;
	switch (actionInstance->ActionCompatibilityMode)
	{
		default:
			break;
		case EActionCompatibilityMode::WhileCompatibleActionOnly:
			{
				incompatible = true;
				if (actionInstance->CompatibleActions.Num() > 0)
				{
					if (ActionInstances.IsValidIndex(actionIndex) && ActionInstances[actionIndex].IsValid())
					{
						const auto actionName = ActionInstances[actionIndex]->GetDescriptionName();
						if (actionInstance->CompatibleActions.Contains(actionName))
						{
							incompatible = false;
						}
					}
				}
			}
			break;
		case EActionCompatibilityMode::OnCompatibleStateOnly:
			{
				incompatible = true;
				if (StatesInstances.IsValidIndex(stateIndex) && actionInstance->CompatibleStates.Num() > 0 && StatesInstances[stateIndex].IsValid())
				{
					const auto stateName = StatesInstances[stateIndex]->GetDescriptionName();
					if (actionInstance->CompatibleStates.Contains(stateName))
					{
						incompatible = false;
					}
				}
			}
			break;
		case EActionCompatibilityMode::OnBothCompatiblesStateAndAction:
			{
				int compatibilityCount = 0;
				//State
				if (StatesInstances.IsValidIndex(stateIndex) && actionInstance->CompatibleStates.Num() > 0 && StatesInstances[stateIndex].IsValid())
				{
					const auto stateName = StatesInstances[stateIndex]->GetDescriptionName();
					if (actionInstance->CompatibleStates.Contains(stateName))
					{
						compatibilityCount++;
					}
				}
				//Actions
				if (actionInstance->CompatibleActions.Num() > 0)
				{
					if (ActionInstances.IsValidIndex(actionIndex) && ActionInstances[actionIndex].IsValid())
					{
						const auto actionName = ActionInstances[actionIndex]->GetDescriptionName();
						if (actionInstance->CompatibleActions.Contains(actionName))
						{
							compatibilityCount++;
						}
					}
				}
				incompatible = compatibilityCount < 2;
			}
			break;
	}

	return !incompatible;
}


bool UModularControllerComponent::PlayActionMontage(FActionMotionMontage Montage, int priority)
{
	if (ActionInstances.Num() <= 0)
		return false;
	UActionMontage* action = Cast<UActionMontage>(ActionInstances[0].Get());
	if (!action)
		return false;
	if (!ActionInfos.Contains(action))
		return false;
	auto infos = ActionInfos[action];
	if (infos.GetRemainingActivationTime() >= 0)
		return false;
	return action->SetActionParams(this, Montage, priority);
}


FControllerCheckResult UModularControllerComponent::TryChangeControllerAction(FControllerStatus toActionStatus, FControllerStatus fromActionStatus)
{
	FControllerCheckResult result = FControllerCheckResult(false, fromActionStatus);
	const bool transitionToSelf = toActionStatus.StatusParams.PrimaryActionFlag > 0;

	const int fromActionIndex = ComputedControllerStatus.StatusParams.ActionIndex;
	const int toActionIndex = toActionStatus.StatusParams.ActionIndex;
	result.ProcessResult.StatusParams.StatusCosmeticVariables = toActionStatus.StatusParams.StatusCosmeticVariables;

	if (fromActionIndex == toActionIndex)
	{
		if (!transitionToSelf)
			return result;
	}

	result.CheckedCondition = true;
	toActionStatus.StatusParams.ActionsModifiers = FVector(0);
	result.ProcessResult = toActionStatus;
	return result;
}


void UModularControllerComponent::ChangeControllerAction(FControllerStatus toActionStatus, const float inDelta)
{
	const int fromActionIndex = ApplyedControllerStatus.StatusParams.ActionIndex;
	const int toActionIndex = toActionStatus.StatusParams.ActionIndex;
	const bool transitionToSelf = toActionStatus.StatusParams.PrimaryActionFlag > 0;

	if (fromActionIndex == toActionIndex)
	{
		if (!transitionToSelf)
		{
			bool repeatAuto = false;
			const auto netRole = GetNetRole(OwnerPawn);
			if ((netRole == ROLE_SimulatedProxy || (netRole == ROLE_Authority && !OwnerPawn->IsLocallyControlled())) &&
				ActionInstances.IsValidIndex(fromActionIndex) && ActionInstances[fromActionIndex].IsValid() && ActionInfos.Contains(ActionInstances[fromActionIndex]))
			{
				if (ActionInfos[ActionInstances[fromActionIndex]].GetRemainingActivationTime() < inDelta
					&& ActionInfos[ActionInstances[fromActionIndex]].CurrentPhase == EActionPhase::Recovery
					&& ActionInstances[fromActionIndex]->bCanTransitionToSelf)
				{
					repeatAuto = true;
				}
			}
			if (!repeatAuto)
				return;
		}
	}

	if (DebugType == EControllerDebugType::StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Trying to change action from: %d to: %d"), fromActionIndex, toActionIndex), true, false
		                                  , FColor::White, 5, "TryChangeControllerActions_1");
	}

	//Disable last action
	if (ActionInstances.IsValidIndex(fromActionIndex) && ActionInstances[fromActionIndex].IsValid())
	{
		ActionInstances[fromActionIndex]->OnActionEnds(this, toActionStatus.Kinematics, toActionStatus.MoveInput, inDelta);
		//if action montage, trigger complete event
		if (fromActionIndex == 0)
		{
			if (UActionMontage* asActionMontage = Cast<UActionMontage>(ActionInstances[fromActionIndex].Get()))
			{
				if (ActionMontageLibraryMap.Contains(asActionMontage->GetDescriptionName()))
				{
					ActionMontageLibraryMap.Remove(asActionMontage->GetDescriptionName());
				}
				asActionMontage->Reset();
				OnActionMontageCompleted.Broadcast();
			}
		}
		//Stop action montage
		if (ActionMontageLibraryMap.Contains(ActionInstances[fromActionIndex]->GetDescriptionName()))
		{
			const auto entry = ActionMontageLibraryMap[ActionInstances[fromActionIndex]->GetDescriptionName()];
			for (const auto actionMontage : entry.Library)
			{
				if (!entry.bOverrideStopOnActionEnds)
					if (!actionMontage.bStopOnActionEnds)
						continue;
				if (actionMontage.bUseMontageLenght)
					continue;
				StopMontage(actionMontage, entry.bOverridePlayOnState ? true : actionMontage.bPlayOnState);
			}
		}

		if (ActionInfos.Contains(ActionInstances[fromActionIndex]))
			ActionInfos[ActionInstances[fromActionIndex]].Reset(ActionInstances[fromActionIndex]->CoolDownDelay);
		if (DebugType == EControllerDebugType::StatusDebug)
		{
			UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Action (%s) is Being Disabled"), *ActionInstances[fromActionIndex]->DebugString()), true, false, FColor::Red,
			                                  5,
			                                  "TryChangeControllerActions_2");
		}
	}

	//Activate action
	if (ActionInstances.IsValidIndex(toActionIndex) && ActionInstances[toActionIndex].IsValid())
	{
		FVector4 actTimings = ActionInstances[toActionIndex]->OnActionBegins(this, toActionStatus.Kinematics, toActionStatus.MoveInput, inDelta);
		//Play Montage
		if (ActionMontageLibraryMap.Contains(ActionInstances[toActionIndex]->GetDescriptionName()))
		{
			const auto actionMontage = UFunctionLibrary::GetActionMontageAt(ActionMontageLibraryMap[ActionInstances[toActionIndex]->GetDescriptionName()], static_cast<int>(actTimings.W));
			if (actionMontage.bUseMontageSectionsAsPhases)
			{
				actTimings = ActionInstances[toActionIndex]->RemapDurationByMontageSections(actionMontage.Montage, actTimings);
			}

			//Play montage
			float montageDuration = 0;
			if (actionMontage.bPlayOnState)
			{
				if (const auto currentState = GetCurrentControllerState())
					montageDuration = PlayAnimationMontageOnState_Internal(actionMontage, currentState->GetDescriptionName(), -1, actionMontage.bUseMontageLenght,
					                                                       _onActionMontageEndedCallBack, _OnActionMontageBlendingOutStartedCallBack);
			}
			else
			{
				montageDuration = PlayAnimationMontage_Internal(actionMontage, -1, actionMontage.bUseMontageLenght, _onActionMontageEndedCallBack,
				                                                _OnActionMontageBlendingOutStartedCallBack);
			}

			if (actionMontage.bUseMontageLenght && montageDuration > 0)
			{
				actTimings = ActionInstances[toActionIndex]->RemapDuration(montageDuration, actTimings);
				if (actionMontage.Montage)
				{
					if (_montageOnActionBound.Contains(actionMontage.Montage))
						_montageOnActionBound[actionMontage.Montage].Add(ActionInstances[toActionIndex]);
					else
						_montageOnActionBound.Add(actionMontage.Montage, TArray<TSoftObjectPtr<UBaseControllerAction>>{ActionInstances[toActionIndex]});
				}
			}
		}

		if (ActionInfos.Contains(ActionInstances[toActionIndex]))
			ActionInfos[ActionInstances[toActionIndex]].Init(actTimings, ActionInstances[toActionIndex]->CoolDownDelay,
			                                                 transitionToSelf ? (ActionInfos[ActionInstances[toActionIndex]]._repeatCount + 1) : 0, static_cast<int>(actTimings.W));
		if (DebugType == EControllerDebugType::StatusDebug)
		{
			UKismetSystemLibrary::PrintString(
				GetWorld(), FString::Printf(TEXT("Action (%s) is Being Activated. Remaining Time: %f"), *ActionInstances[toActionIndex]->DebugString(),
				                            ActionInfos.Contains(ActionInstances[toActionIndex]) ? ActionInfos[ActionInstances[toActionIndex]].GetRemainingActivationTime() : -1), true,
				false,
				FColor::Green, 5
				, "TryChangeControllerActions_3");
		}
	}

	OnControllerActionChanged(ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex].Get() : nullptr
	                          , ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex].Get() : nullptr);
	OnControllerActionChangedEvent.Broadcast(ActionInstances.IsValidIndex(toActionIndex) ? ActionInstances[toActionIndex].Get() : nullptr
	                                         , ActionInstances.IsValidIndex(fromActionIndex) ? ActionInstances[fromActionIndex].Get() : nullptr);

	if (DebugType == EControllerDebugType::StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Changed actions from: %d  to: %d"), fromActionIndex, toActionIndex), true, false
		                                  , FColor::Yellow, 5, TEXT("TryChangeControllerActions_4"));
	}
}


FControllerStatus UModularControllerComponent::ProcessControllerAction(const FControllerStatus initialState, const float inDelta)
{
	FControllerStatus processMotion = initialState;
	const int index = initialState.StatusParams.ActionIndex;

	if (ActionInstances.IsValidIndex(index) && ActionInstances[index].IsValid())
	{
		processMotion = ProcessSingleAction(ActionInstances[index], processMotion, inDelta);

		if (DebugType == EControllerDebugType::StatusDebug)
		{
			UKismetSystemLibrary::PrintString(
				GetWorld(), FString::Printf(TEXT("Action (%s) is Being Processed. Phase: %s Remaining Total Time: %f. Montage Weight: %f"), *ActionInstances[index]->DebugString(),
				                            ActionInfos.Contains(ActionInstances[index])
					                            ? *UEnum::GetValueAsString(ActionInfos[ActionInstances[index]].CurrentPhase)
					                            : *FString::Printf(TEXT("None")),
				                            ActionInfos.Contains(ActionInstances[index]) ? ActionInfos[ActionInstances[index]].GetRemainingActivationTime() : -1,
				                            UFunctionLibrary::GetMontageCurrentWeight(GetAnimInstance(), GetActionCurrentMotionMontage(ActionInstances[index].Get()).Montage))
				, true, false, FColor::White, 5, "ProcessControllerActions");
		}
	}

	return processMotion;
}


FControllerStatus UModularControllerComponent::ProcessSingleAction(TSoftObjectPtr<UBaseControllerAction> actionInstance, const FControllerStatus initialState, const float inDelta)
{
	if (!actionInstance.IsValid())
		return initialState;

	FControllerStatus processMotion = initialState;
	switch (ActionInfos[actionInstance].CurrentPhase)
	{
		case EActionPhase::Anticipation:
			processMotion = actionInstance->OnActionProcessAnticipationPhase(this, processMotion, ActionInfos[actionInstance], inDelta);
			break;
		case EActionPhase::Active:
			processMotion = actionInstance->OnActionProcessActivePhase(this, processMotion, ActionInfos[actionInstance], inDelta);
			break;
		case EActionPhase::Recovery:
			processMotion = actionInstance->OnActionProcessRecoveryPhase(this, processMotion, ActionInfos[actionInstance], inDelta);
			break;
		default: break;
	}

	return processMotion;
}


void UModularControllerComponent::OnControllerActionChanged_Implementation(UBaseControllerAction* newAction, UBaseControllerAction* lastAction)
{
}


#pragma endregion


#pragma region Watchers


bool UModularControllerComponent::CheckTraversalWatcherByType(TSubclassOf<UBaseTraversalWatcher> moduleType) const
{
	if (WatcherInstances.Num() <= 0)
		return false;
	const auto index = WatcherInstances.IndexOfByPredicate([moduleType](const TSoftObjectPtr<UBaseTraversalWatcher>& watcher)
	{
		return watcher.IsValid() && watcher->GetClass() == moduleType;
	});
	return WatcherInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckTraversalWatcherByName(FName moduleName) const
{
	if (WatcherInstances.Num() <= 0)
		return false;
	const auto index = WatcherInstances.IndexOfByPredicate([moduleName](const TSoftObjectPtr<UBaseTraversalWatcher>& watcher)
	{
		return watcher.IsValid() && watcher->GetDescriptionName() == moduleName;
	});
	return WatcherInstances.IsValidIndex(index);
}

bool UModularControllerComponent::CheckTraversalWatcherByPriority(int modulePriority) const
{
	if (WatcherInstances.Num() <= 0)
		return false;
	const auto index = WatcherInstances.IndexOfByPredicate([modulePriority](const TSoftObjectPtr<UBaseTraversalWatcher>& watcher)
	{
		return watcher.IsValid() && watcher->GetPriority() == modulePriority;
	});
	return WatcherInstances.IsValidIndex(index);
}

void UModularControllerComponent::SortTraversalWatchers()
{
	if (WatcherInstances.Num() > 1)
	{
		WatcherInstances.Sort([](const TSoftObjectPtr<UBaseTraversalWatcher>& a, const TSoftObjectPtr<UBaseTraversalWatcher>& b)
		{
			return a.IsValid() && b.IsValid() && a->GetPriority() > b->GetPriority();
		});
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::AddTraversalWatcher_Implementation(TSubclassOf<UBaseTraversalWatcher> moduleType)
{
	if (!moduleType)
		return;
	if (CheckTraversalWatcherByType(moduleType.Get()))
		return;
	WatcherInstances.Add(moduleType->GetDefaultObject());
	SortTraversalWatchers();
}

UBaseTraversalWatcher* UModularControllerComponent::GetTraversalWatcherByType(TSubclassOf<UBaseTraversalWatcher> moduleType)
{
	if (WatcherInstances.Num() <= 0)
		return nullptr;
	const auto index = WatcherInstances.IndexOfByPredicate([moduleType](const TSoftObjectPtr<UBaseTraversalWatcher>& watcher)
	{
		return watcher.IsValid() && watcher->GetClass() == moduleType;
	});
	if (WatcherInstances.IsValidIndex(index))
	{
		return WatcherInstances[index].Get();
	}
	return nullptr;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::RemoveTraversalWatcherByType_Implementation(TSubclassOf<UBaseTraversalWatcher> moduleType)
{
	if (CheckTraversalWatcherByType(moduleType))
	{
		auto watcher = WatcherInstances.FindByPredicate([moduleType](const TSoftObjectPtr<UBaseTraversalWatcher>& watch)
		{
			return watch.IsValid() && watch->GetClass() == moduleType->GetClass();
		});
		WatcherInstances.Remove(*watcher);
		SortTraversalWatchers();
	}
}

void UModularControllerComponent::RemoveTraversalWatcherByName_Implementation(FName moduleName)
{
	if (CheckTraversalWatcherByName(moduleName))
	{
		auto watcher = WatcherInstances.FindByPredicate([moduleName](const TSoftObjectPtr<UBaseTraversalWatcher>& watch)
		{
			return watch.IsValid() && watch->GetDescriptionName() == moduleName;
		});
		WatcherInstances.Remove(*watcher);
		SortTraversalWatchers();
	}
}

void UModularControllerComponent::RemoveTraversalWatcherByPriority_Implementation(int modulePriority)
{
	if (CheckTraversalWatcherByPriority(modulePriority))
	{
		auto watcher = WatcherInstances.FindByPredicate([modulePriority](const TSoftObjectPtr<UBaseTraversalWatcher>& watch)
		{
			return watch.IsValid() && watch->GetPriority() == modulePriority;
		});
		WatcherInstances.Remove(*watcher);
		SortTraversalWatchers();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UModularControllerComponent::CheckControllerTraversalWatcher(FControllerStatus currentControllerStatus, const float inDelta)
{
	if (CurrentPerformanceProfile.TraversalTickInterval > 0)
	{
		if (_watcherTickChrono < CurrentPerformanceProfile.TraversalTickInterval)
		{
			_watcherTickChrono += inDelta;
			bool skipReturn = false;
			if (_watcherTickChrono >= CurrentPerformanceProfile.TraversalTickInterval)
			{
				_watcherTickChrono = 0;
				skipReturn = true;
			}

			if (!skipReturn)
				return;
		}
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CheckControllerTraversalWatcher");
	TQueue<FTraversalCommandParams> commandsQueue;
	int selectedWatcherIndex = -1;
	TMap<FName, TMap<FName, TArray<bool>>> _motherTraversalDebug;
	if (DebugType == EControllerDebugType::TraversalDebug)
		_motherTraversalDebug = TMap<FName, TMap<FName, TArray<bool>>>();
	bool actionMontageActive = IsActionMontagePlaying();

	//Check transversal watcher
	{
		if (CurrentPerformanceProfile.bUseMultiThreadTraversal)
		{
			ParallelFor(StatesInstances.Num(), [&](int32 i)
			{
				if (!WatcherInstances.IsValidIndex(i))
					return;

				if (!WatcherInstances[i].IsValid())
					return;

				if(actionMontageActive && !WatcherInstances[i]->bIgnoreActionMontage)
					return;

				if (CheckTraversalWatcherCompatibility(WatcherInstances[i], currentControllerStatus.StatusParams.StateIndex, currentControllerStatus.StatusParams.ActionIndex))
				{
					if (DebugType == EControllerDebugType::TraversalDebug)
						_motherTraversalDebug.Add(WatcherInstances[i]->GetDescriptionName(), TMap<FName, TArray<bool>>());

					if (WatcherInstances[i]->CheckWatcher(commandsQueue, this, currentControllerStatus, inDelta,
					                                      DebugType == EControllerDebugType::TraversalDebug
						                                      ? &_motherTraversalDebug[WatcherInstances[i]->GetDescriptionName()]
						                                      : nullptr))
					{
						if (DebugType == EControllerDebugType::StatusDebug)
						{
							UKismetSystemLibrary::PrintString(
								GetWorld(), FString::Printf(TEXT("Traversal Watcher (%s) was checked as active."), *WatcherInstances[i]->DebugString()), true, false, FColor::Silver
								, 0, FName(FString::Printf(TEXT("CheckControllerWatcher_%s"), *WatcherInstances[i]->GetDescriptionName().ToString())));
						}
					}
				}
			}, EParallelForFlags::BackgroundPriority);
		}
		else
		{
			for (int i = 0; i < WatcherInstances.Num(); i++)
			{
				if (selectedWatcherIndex == i)
					continue;

				if (!WatcherInstances[i].IsValid())
					continue;
				
				if(actionMontageActive && !WatcherInstances[i]->bIgnoreActionMontage)
					continue;;

				if (CheckTraversalWatcherCompatibility(WatcherInstances[i], currentControllerStatus.StatusParams.StateIndex, currentControllerStatus.StatusParams.ActionIndex))
				{
					if (WatcherInstances.IsValidIndex(selectedWatcherIndex) && WatcherInstances[selectedWatcherIndex].IsValid())
					{
						if (WatcherInstances[i]->GetPriority() <= WatcherInstances[selectedWatcherIndex]->GetPriority())
						{
							continue;
						}
					}

					if (DebugType == EControllerDebugType::TraversalDebug)
						_motherTraversalDebug.Add(WatcherInstances[i]->GetDescriptionName(), TMap<FName, TArray<bool>>());

					if (WatcherInstances[i]->CheckWatcher(commandsQueue, this, currentControllerStatus, inDelta,
					                                      DebugType == EControllerDebugType::TraversalDebug
						                                      ? &_motherTraversalDebug[WatcherInstances[i]->GetDescriptionName()]
						                                      : nullptr))
					{
						selectedWatcherIndex = i;
						if (DebugType == EControllerDebugType::StatusDebug)
						{
							UKismetSystemLibrary::PrintString(
								GetWorld(), FString::Printf(TEXT("Traversal Watcher (%s) was checked as active."), *WatcherInstances[i]->DebugString()), true, false, FColor::Silver
								, 0, FName(FString::Printf(TEXT("CheckControllerWatcher_%s"), *WatcherInstances[i]->GetDescriptionName().ToString())));
						}
					}
				}
			}
		}
	}

	FTraversalCommandParams command;
	while (commandsQueue.Dequeue(command))
	{
		OnControllerTriggerTraversalEvent.Broadcast(command.ParamKey, command.PathPoints);
	}

	if (DebugType == EControllerDebugType::StatusDebug)
	{
		UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Check Watchers: selected Index (%d)"), selectedWatcherIndex), true, false, FColor::Silver, 0
		                                  , TEXT("CheckControllerWatcher"));
	}

	if (DebugType == EControllerDebugType::TraversalDebug)
	{
		float duration = 60;
		for (auto motherItem : _motherTraversalDebug)
		{
			auto debugName = FString::Printf(TEXT("CheckControllerWatcher_%s"), *motherItem.Key.ToString());
			FString innerContent = FString();
			for (auto childItem : motherItem.Value)
			{
				innerContent = FString::Printf(
					TEXT(
						"Key(%s)  Tested(%d)  AsPrediction(%d)  SurfaceCount(%d)  SurfaceValid(%d)  ColResponse(%d)  canStep(%d)  Height(%d)  nAngle(%d)  iAngle(%d)  awayTest(%d)  orientation(%d)  depth(%d)  axisSpeed(%d)  surfaceDirSpeed(%d)  SurfaceSpeed(%d)  Cosmetic(%d)")
					, *childItem.Key.ToString(), childItem.Value[1], childItem.Value[2], childItem.Value[3], childItem.Value[4], childItem.Value[5], childItem.Value[6],
					childItem.Value[7],
					childItem.Value[8], childItem.Value[9], childItem.Value[10], childItem.Value[11], childItem.Value[12], childItem.Value[13], childItem.Value[14],
					childItem.Value[15],
					childItem.Value[16]
				);
				auto nameKey = debugName;
				UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("%s:"), *innerContent), true, false, childItem.Value[0] ? FColor::Green : FColor::Silver
				                                  , duration, FName(nameKey.Append(childItem.Key.ToString())));
			}
			UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Check Watchers full status Tab for %s:")
			                                                              , *motherItem.Key.ToString()), true, false, FColor::White, duration, FName(debugName));
		}
	}
}

bool UModularControllerComponent::CheckTraversalWatcherCompatibility(const TSoftObjectPtr<UBaseTraversalWatcher> watcherInstance, int stateIndex, int actionIndex) const
{
	if (!watcherInstance.IsValid())
		return false;

	bool incompatible = false;
	switch (watcherInstance->WatcherCompatibilityMode)
	{
		default:
			break;
		case EActionCompatibilityMode::WhileCompatibleActionOnly:
			{
				incompatible = true;
				if (watcherInstance->CompatibleActions.Num() > 0)
				{
					if (ActionInstances.IsValidIndex(actionIndex) && ActionInstances[actionIndex].IsValid())
					{
						const auto actionName = ActionInstances[actionIndex]->GetDescriptionName();
						if (watcherInstance->CompatibleActions.Contains(actionName))
						{
							incompatible = false;
						}
					}
				}
			}
			break;
		case EActionCompatibilityMode::OnCompatibleStateOnly:
			{
				incompatible = true;
				if (StatesInstances.IsValidIndex(stateIndex) && watcherInstance->CompatibleStates.Num() > 0 && StatesInstances[stateIndex].IsValid())
				{
					const auto stateName = StatesInstances[stateIndex]->GetDescriptionName();
					if (watcherInstance->CompatibleStates.Contains(stateName))
					{
						incompatible = false;
					}
				}
			}
			break;
		case EActionCompatibilityMode::OnBothCompatiblesStateAndAction:
			{
				int compatibilityCount = 0;
				//State
				if (StatesInstances.IsValidIndex(stateIndex) && watcherInstance->CompatibleStates.Num() > 0 && StatesInstances[stateIndex].IsValid())
				{
					const auto stateName = StatesInstances[stateIndex]->GetDescriptionName();
					if (watcherInstance->CompatibleStates.Contains(stateName))
					{
						compatibilityCount++;
					}
				}
				//Actions
				if (watcherInstance->CompatibleActions.Num() > 0)
				{
					if (ActionInstances.IsValidIndex(actionIndex) && ActionInstances[actionIndex].IsValid())
					{
						const auto actionName = ActionInstances[actionIndex]->GetDescriptionName();
						if (watcherInstance->CompatibleActions.Contains(actionName))
						{
							compatibilityCount++;
						}
					}
				}
				incompatible = compatibilityCount < 2;
			}
			break;
	}

	return !incompatible;
}


#pragma endregion
