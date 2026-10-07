// Copyright Preetham Mukundan (C) 2026

#include "Animation/AnimNode_ZLDeadlockHero.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTrace.h"
#include "Animation/ZL_DeadlockAnimSet.h"
#include "AnimationRuntime.h"

namespace ZLDeadlockHero
{
	constexpr int32 MaxLayers = 4;
	constexpr float MinWeight = 0.001f;
	constexpr float LandBlendTime = 0.06f;
	constexpr float DashBlendTime = 0.08f;
	/** A slide starting within this long after being airborne counts as landing into the slide. */
	constexpr float SlideFromAirWindow = 0.25f;

	float Length(const UAnimSequence* Seq)
	{
		return Seq ? FMath::Max(Seq->GetPlayLength(), KINDA_SMALL_NUMBER) : 0.f;
	}

	bool StateUsesAimOffset(EZLDeadlockPoseState State)
	{
		return State == EZLDeadlockPoseState::Ground || State == EZLDeadlockPoseState::Crouch
			|| State == EZLDeadlockPoseState::Air || State == EZLDeadlockPoseState::Land;
	}

	void CopyPose(FPoseContext& To, const FPoseContext& From)
	{
		To.Pose.CopyBonesFrom(From.Pose);
		To.Curve.CopyFrom(From.Curve);
		To.CustomAttributes.CopyFrom(From.CustomAttributes);
	}
}

// ------------------------------------------------------------------------------------------------ init / bones

void FAnimNode_ZLDeadlockHero::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	Layers.Reset();
	PushLayer(EZLDeadlockPoseState::Ground, 0.f);
	Phase = 0.f;
	MoveAlpha = 0.f;
	MoveDir = FVector2D(1.f, 0.f);
	AimWeight = 0.f;
	SmoothedAimPitch = 0.f;
	TimeSinceAirborne = 10.f;
	UpperBodyWeights = FSlotWeights();
	FullBodyWeights = FSlotWeights();

	// Don't replay jumps/landings that happened before this node existed.
	if (const UZL_DeadlockAnimInstance* Instance = Cast<UZL_DeadlockAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()))
	{
		SeenJumpCount = Instance->State.JumpCount;
		SeenLandCount = Instance->State.LandCount;
		SeenDashCount = Instance->State.DashCount;
	}

	if (!SlotNodeInitializationCounter.IsSynchronized_Counter(Context.AnimInstanceProxy->GetSlotNodeInitializationCounter()))
	{
		SlotNodeInitializationCounter.SynchronizeWith(Context.AnimInstanceProxy->GetSlotNodeInitializationCounter());
		if (!UpperBodySlot.IsNone())
		{
			Context.AnimInstanceProxy->RegisterSlotNodeWithAnimInstance(UpperBodySlot);
		}
		if (!FullBodySlot.IsNone())
		{
			Context.AnimInstanceProxy->RegisterSlotNodeWithAnimInstance(FullBodySlot);
		}
	}
}

void FAnimNode_ZLDeadlockHero::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_Base::CacheBones_AnyThread(Context);

	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	const int32 NumBones = Bones.GetCompactPoseNumBones();

	auto FindBone = [&Bones](FName Name)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};

	// 1 for the bone and everything below it. Compact poses list parents before children.
	auto BuildBranchWeights = [&](FName RootName, TArray<float>& OutWeights)
	{
		const FCompactPoseBoneIndex Root = FindBone(RootName);
		OutWeights.Init(Root.IsValid() ? 0.f : 1.f, NumBones);
		if (!Root.IsValid())
		{
			return;
		}
		for (int32 Index = 0; Index < NumBones; ++Index)
		{
			const FCompactPoseBoneIndex Bone(Index);
			if (Bone == Root)
			{
				OutWeights[Index] = 1.f;
			}
			else
			{
				const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
				OutWeights[Index] = Parent.IsValid() ? OutWeights[Parent.GetInt()] : 0.f;
			}
		}
	};

	BuildBranchWeights(UpperBodyRootBone, UpperBodyBoneWeights);
	BuildBranchWeights(AimRootBone, AimBoneWeights);

	RootMotionIndex = FindBone(RootMotionBone);
	RootSiblings.Reset();
	if (RootMotionIndex.IsValid())
	{
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(RootMotionIndex);
		for (int32 Index = 0; Index < NumBones; ++Index)
		{
			const FCompactPoseBoneIndex Bone(Index);
			if (Bone != RootMotionIndex && Bones.GetParentBoneIndex(Bone) == Parent)
			{
				RootSiblings.Add(Bone);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------ update

EZLDeadlockPoseState FAnimNode_ZLDeadlockHero::ChooseState() const
{
	if (State.bZiplining && (Set->ZiplineLoop || Set->ZiplineStart))
	{
		return EZLDeadlockPoseState::Zipline;
	}
	if (State.bSliding && (Set->SlideLoop || Set->SlideStart))
	{
		return EZLDeadlockPoseState::Slide;
	}
	if (State.bWallJumping)
	{
		return EZLDeadlockPoseState::WallJump;
	}
	if (!State.bOnGround && !State.bSliding && !State.bZiplining)
	{
		return EZLDeadlockPoseState::Air;
	}
	return State.bCrouching || State.bSliding ? EZLDeadlockPoseState::Crouch : EZLDeadlockPoseState::Ground;
}

void FAnimNode_ZLDeadlockHero::PushLayer(EZLDeadlockPoseState NewState, float BlendTime, UAnimSequence* Clip, float StartTime)
{
	if (Layers.Num() >= ZLDeadlockHero::MaxLayers)
	{
		Layers.RemoveAt(0);
	}
	FLayer& Layer = Layers.AddDefaulted_GetRef();
	Layer.State = NewState;
	Layer.BlendInTime = BlendTime;
	Layer.Weight = BlendTime <= 0.f || Layers.Num() == 1 ? 1.f : 0.f;
	Layer.Clip = Clip;
	Layer.Time = StartTime;
}

UAnimSequence* FAnimNode_ZLDeadlockHero::ChooseDashClip() const
{
	const FVector& Dir = State.DashDirection;
	UAnimSequence* Clip = nullptr;
	if (FMath::Abs(Dir.X) >= FMath::Abs(Dir.Y))
	{
		Clip = Dir.X >= 0.f ? (State.bOnGround && Set->DashGround ? Set->DashGround.Get() : Set->DashForward.Get()) : Set->DashBack.Get();
	}
	else
	{
		Clip = Dir.Y >= 0.f ? Set->DashRight.Get() : Set->DashLeft.Get();
	}
	return Clip ? Clip : (Set->DashForward ? Set->DashForward.Get() : Set->DashGround.Get());
}

float FAnimNode_ZLDeadlockHero::GetLayerHoldTime(const FLayer& Layer) const
{
	return FMath::Min(ZLDeadlockHero::Length(Layer.Clip), Set->LandHoldTime);
}

void FAnimNode_ZLDeadlockHero::UpdateLayerWeights(float InDeltaTime)
{
	FLayer& Top = Layers.Last();
	Top.Weight = Top.BlendInTime > 0.f ? FMath::Min(1.f, Top.Weight + InDeltaTime / Top.BlendInTime) : 1.f;

	// Older layers share whatever the newest one hasn't taken yet, keeping their relative weights.
	float OthersTotal = 0.f;
	for (int32 Index = 0; Index < Layers.Num() - 1; ++Index)
	{
		OthersTotal += Layers[Index].Weight;
	}
	if (OthersTotal <= ZLDeadlockHero::MinWeight)
	{
		Top.Weight = 1.f;
	}
	const float Remaining = 1.f - Top.Weight;
	for (int32 Index = Layers.Num() - 2; Index >= 0; --Index)
	{
		FLayer& Layer = Layers[Index];
		Layer.Weight = OthersTotal > ZLDeadlockHero::MinWeight ? Layer.Weight * Remaining / OthersTotal : 0.f;
		if (Layer.Weight < ZLDeadlockHero::MinWeight)
		{
			Layers.RemoveAt(Index);
		}
	}
}

void FAnimNode_ZLDeadlockHero::UpdateLocomotion(float InDeltaTime)
{
	const FVector2D Velocity(State.LocalVelocity.X, State.LocalVelocity.Y);
	if (State.GroundSpeed > 10.f)
	{
		// Smooth the move direction so diagonal changes blend instead of popping.
		const FVector2D Target = Velocity.GetSafeNormal();
		const FVector2D Blended = MoveDir + (Target - MoveDir) * FMath::Clamp(InDeltaTime * 12.f, 0.f, 1.f);
		MoveDir = Blended.IsNearlyZero() ? Target : Blended.GetSafeNormal();
	}

	MoveAlpha = FMath::FInterpTo(MoveAlpha, FMath::Clamp(State.GroundSpeed / Set->MoveBlendSpeed, 0.f, 1.f), InDeltaTime, 10.f);

	// One shared phase for stand and crouch cycles keeps the feet in step across crouch transitions.
	const bool bCrouch = State.bCrouching && Set->CrouchRun.HasAny();
	const FZLDirectionalAnims& Cycle = bCrouch ? Set->CrouchRun : Set->Run;
	const float Authored = FMath::Max(bCrouch ? State.CrouchAuthoredSpeed : State.RunAuthoredSpeed, 1.f);
	const float PlayRate = State.GroundSpeed > 1.f ? FMath::Clamp(State.GroundSpeed / Authored, Set->MinPlayRate, Set->MaxPlayRate) : 1.f;
	const float CycleLength = ZLDeadlockHero::Length(Cycle.GetWithFallback(0));
	if (CycleLength > 0.f)
	{
		Phase = FMath::Fmod(Phase + InDeltaTime * PlayRate / CycleLength, 1.f);
	}
}

void FAnimNode_ZLDeadlockHero::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);
	DeltaTime = Context.GetDeltaTime();

	const UZL_DeadlockAnimInstance* Instance = Cast<UZL_DeadlockAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	if (Instance)
	{
		State = Instance->State;
	}
	Set = AnimSetOverride ? AnimSetOverride.Get() : (Instance ? Instance->GetActiveAnimSet() : nullptr);

	if (Set)
	{
		const bool bJumped = State.JumpCount != SeenJumpCount;
		const bool bLanded = State.LandCount != SeenLandCount;
		const bool bDashed = State.DashCount != SeenDashCount;
		SeenJumpCount = State.JumpCount;
		SeenLandCount = State.LandCount;
		SeenDashCount = State.DashCount;

		const bool bAirborne = !State.bOnGround && !State.bSliding && !State.bZiplining;
		TimeSinceAirborne = bAirborne ? 0.f : TimeSinceAirborne + DeltaTime;

		// A dash plays its clip for DashHoldTime, then hands back to whatever state we're in by then.
		if (bDashed && !State.bSliding && !State.bZiplining)
		{
			if (UAnimSequence* DashClip = ChooseDashClip())
			{
				PushLayer(EZLDeadlockPoseState::Dash, ZLDeadlockHero::DashBlendTime, DashClip);
			}
		}
		const FLayer& TopLayer = Layers.Last();
		const bool bHoldingDash = TopLayer.State == EZLDeadlockPoseState::Dash
			&& (TopLayer.Time < Set->DashHoldTime || State.bDashing) && !bJumped;

		const EZLDeadlockPoseState Desired = bHoldingDash ? EZLDeadlockPoseState::Dash : ChooseState();
		const EZLDeadlockPoseState Current = TopLayer.State;

		switch (Desired)
		{
		case EZLDeadlockPoseState::Air:
			if (bJumped || Current != EZLDeadlockPoseState::Air)
			{
				// Walking off a ledge has no jump clip; a jump (or another jump mid-air) restarts it.
				UAnimSequence* Clip = nullptr;
				if (bJumped)
				{
					Clip = State.bLastJumpWasAirJump && Set->AirJumpStart ? Set->AirJumpStart.Get() : Set->JumpStart.Get();
				}
				PushLayer(EZLDeadlockPoseState::Air, Clip ? Set->JumpBlendTime : Set->StateBlendTime, Clip);
			}
			break;

		case EZLDeadlockPoseState::WallJump:
			if (bJumped || Current != EZLDeadlockPoseState::WallJump)
			{
				UAnimSequence* Clip = Set->WallJump ? Set->WallJump.Get() : (Set->AirJumpStart ? Set->AirJumpStart.Get() : Set->JumpStart.Get());
				PushLayer(EZLDeadlockPoseState::WallJump, Set->JumpBlendTime, Clip);
			}
			break;

		case EZLDeadlockPoseState::Dash:
			break;

		case EZLDeadlockPoseState::Slide:
			if (Current != EZLDeadlockPoseState::Slide)
			{
				// Landing into a slide: skip SlideStart's hop and blend in fast, so the air pose doesn't linger.
				// From a run the intro plays from the start; the hop is what that clip is for.
				const bool bFromAir = TimeSinceAirborne < ZLDeadlockHero::SlideFromAirWindow;
				PushLayer(EZLDeadlockPoseState::Slide, bFromAir ? Set->JumpBlendTime : Set->StateBlendTime, nullptr,
					bFromAir ? State.SlideFromAirStartTime : 0.f);
			}
			break;

		case EZLDeadlockPoseState::Ground:
		case EZLDeadlockPoseState::Crouch:
		{
			const bool bWasAirborne = Current == EZLDeadlockPoseState::Air || Current == EZLDeadlockPoseState::WallJump || Current == EZLDeadlockPoseState::Dash;
			const bool bMoving = State.GroundSpeed > Set->MoveBlendSpeed;
			UAnimSequence* LandClip = State.LastLandingSpeed > Set->HardLandSpeed && Set->HardLand ? Set->HardLand.Get() : Set->Land.Get();
			if (bLanded && bWasAirborne && !bMoving && LandClip)
			{
				PushLayer(EZLDeadlockPoseState::Land, ZLDeadlockHero::LandBlendTime, LandClip);
			}
			else if (Current == EZLDeadlockPoseState::Land)
			{
				const FLayer& Land = Layers.Last();
				if (Land.Time >= GetLayerHoldTime(Land) || (bMoving && Land.Time > 0.1f))
				{
					PushLayer(Desired, Set->StateBlendTime);
				}
			}
			else if (Current != Desired)
			{
				PushLayer(Desired, Set->StateBlendTime);
			}
			break;
		}

		default:
			if (Current != Desired)
			{
				PushLayer(Desired, Set->StateBlendTime);
			}
			break;
		}

		for (FLayer& Layer : Layers)
		{
			Layer.Time += DeltaTime;
		}
		UpdateLayerWeights(DeltaTime);
		UpdateLocomotion(DeltaTime);

		float TargetAimWeight = 0.f;
		if (Set->bEnableAimOffset)
		{
			for (const FLayer& Layer : Layers)
			{
				TargetAimWeight += ZLDeadlockHero::StateUsesAimOffset(Layer.State) ? Layer.Weight : 0.f;
			}
		}
		AimWeight = FMath::FInterpTo(AimWeight, TargetAimWeight, DeltaTime, 10.f);
		SmoothedAimPitch = FMath::FInterpTo(SmoothedAimPitch, State.AimPitch, DeltaTime, 15.f);
	}

	// Montage slots, same bookkeeping as FAnimNode_Slot.
	if (!UpperBodySlot.IsNone())
	{
		Context.AnimInstanceProxy->GetSlotWeight(UpperBodySlot, UpperBodyWeights.SlotNodeWeight, UpperBodyWeights.SourceWeight, UpperBodyWeights.TotalNodeWeight);
		Context.AnimInstanceProxy->UpdateSlotNodeWeight(UpperBodySlot, UpperBodyWeights.SlotNodeWeight, Context.GetFinalBlendWeight());
	}
	if (!FullBodySlot.IsNone())
	{
		Context.AnimInstanceProxy->GetSlotWeight(FullBodySlot, FullBodyWeights.SlotNodeWeight, FullBodyWeights.SourceWeight, FullBodyWeights.TotalNodeWeight);
		Context.AnimInstanceProxy->UpdateSlotNodeWeight(FullBodySlot, FullBodyWeights.SlotNodeWeight, Context.GetFinalBlendWeight());
	}

	TRACE_ANIM_NODE_VALUE(Context, TEXT("State"), *UEnum::GetValueAsString(Layers.Num() ? Layers.Last().State : EZLDeadlockPoseState::Ground));
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Anim Set"), *GetNameSafe(Set));
}

// ------------------------------------------------------------------------------------------------ sampling

bool FAnimNode_ZLDeadlockHero::SamplePose(FPoseContext& Output, const UAnimSequence* Seq, float Time, bool bLoop)
{
	if (!Seq)
	{
		return false;
	}
	const float Length = Seq->GetPlayLength();
	float SampleTime = 0.f;
	if (Length > KINDA_SMALL_NUMBER)
	{
		SampleTime = bLoop ? FMath::Fmod(FMath::Fmod(Time, Length) + Length, Length) : FMath::Clamp(Time, 0.f, Length);
	}
	FAnimationPoseData PoseData(Output);
	Seq->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(SampleTime), false, FDeltaTimeRecord(), bLoop));
	return true;
}

void FAnimNode_ZLDeadlockHero::SampleBlend(FPoseContext& Output, const UAnimSequence* A, float TimeA, bool bLoopA, const UAnimSequence* B, float TimeB, bool bLoopB, float BAlpha)
{
	if (!A)
	{
		SamplePose(Output, B, TimeB, bLoopB);
		return;
	}
	SamplePose(Output, A, TimeA, bLoopA);
	if (B && BAlpha > ZLDeadlockHero::MinWeight && (B != A || TimeA != TimeB))
	{
		FPoseContext Other(Output);
		SamplePose(Other, B, TimeB, bLoopB);
		FAnimationRuntime::LerpPoses(Output.Pose, Other.Pose, Output.Curve, Other.Curve, BAlpha);
	}
}

void FAnimNode_ZLDeadlockHero::EvaluateLocomotion(FPoseContext& Output, UAnimSequence* Idle, const FZLDirectionalAnims& Cycle, float IdleTime) const
{
	// Pick the two direction clips either side of the move direction (0 = forward, clockwise).
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(MoveDir.Y, MoveDir.X));
	const float Sector = FMath::Fmod(Angle / 45.f + 8.f, 8.f);
	const int32 Index = FMath::FloorToInt(Sector);
	const float Alpha = Sector - Index;
	const UAnimSequence* DirA = Cycle.GetWithFallback(Index);
	const UAnimSequence* DirB = Cycle.GetWithFallback(Index + 1);

	if (MoveAlpha < 0.01f || !DirA)
	{
		if (!SamplePose(Output, Idle, IdleTime, true))
		{
			Output.ResetToRefPose();
		}
		return;
	}

	SampleBlend(Output, DirA, Phase * ZLDeadlockHero::Length(DirA), true, DirB, Phase * ZLDeadlockHero::Length(DirB), true, Alpha);
	if (MoveAlpha < 0.99f && Idle)
	{
		FPoseContext IdlePose(Output);
		SamplePose(IdlePose, Idle, IdleTime, true);
		FAnimationRuntime::LerpPoses(Output.Pose, IdlePose.Pose, Output.Curve, IdlePose.Curve, 1.f - MoveAlpha);
	}
}

void FAnimNode_ZLDeadlockHero::EvaluateIntoLoop(FPoseContext& Output, UAnimSequence* Intro, UAnimSequence* LoopA, UAnimSequence* LoopB, float LoopBAlpha, float Time) const
{
	// Play Intro once, crossfading into the loop over its last 0.2s.
	const float IntroLength = ZLDeadlockHero::Length(Intro);
	const float Crossfade = FMath::Min(0.2f, IntroLength * 0.5f);
	const float IntroWeight = Intro ? 1.f - FMath::Clamp((Time - (IntroLength - Crossfade)) / FMath::Max(Crossfade, KINDA_SMALL_NUMBER), 0.f, 1.f) : 0.f;

	if (IntroWeight >= 0.999f || (!LoopA && !LoopB))
	{
		if (!SamplePose(Output, Intro, Time, false) && !SamplePose(Output, Set->StandIdle, Time, true))
		{
			Output.ResetToRefPose();
		}
		return;
	}

	const float LoopTime = Intro ? FMath::Max(0.f, Time - (IntroLength - Crossfade)) : Time;
	if (LoopA)
	{
		SampleBlend(Output, LoopA, LoopTime, true, LoopB, LoopTime, true, LoopBAlpha);
	}
	else
	{
		SamplePose(Output, LoopB, LoopTime, true);
	}

	if (IntroWeight > ZLDeadlockHero::MinWeight)
	{
		FPoseContext IntroPose(Output);
		SamplePose(IntroPose, Intro, Time, false);
		FAnimationRuntime::LerpPoses(Output.Pose, IntroPose.Pose, Output.Curve, IntroPose.Curve, IntroWeight);
	}
}

void FAnimNode_ZLDeadlockHero::EvaluateLayer(const FLayer& Layer, FPoseContext& Output) const
{
	const float FallAlpha = FMath::Clamp(-State.VerticalSpeed / Set->FallBlendSpeed, 0.f, 1.f);

	switch (Layer.State)
	{
	case EZLDeadlockPoseState::Ground:
		EvaluateLocomotion(Output, Set->StandIdle, Set->Run, Layer.Time);
		break;

	case EZLDeadlockPoseState::Crouch:
		EvaluateLocomotion(Output, Set->CrouchIdle ? Set->CrouchIdle.Get() : Set->StandIdle.Get(),
			Set->CrouchRun.HasAny() ? Set->CrouchRun : Set->Run, Layer.Time);
		break;

	case EZLDeadlockPoseState::Air:
	case EZLDeadlockPoseState::WallJump:
		EvaluateIntoLoop(Output, Layer.Clip, Set->InAirApex, Set->InAirFall, FallAlpha, Layer.Time);
		break;

	case EZLDeadlockPoseState::Land:
	case EZLDeadlockPoseState::Dash:
	{
		const float Time = Layer.State == EZLDeadlockPoseState::Dash ? Layer.Time * Set->DashPlayRate : Layer.Time;
		if (!SamplePose(Output, Layer.Clip, Time, false) && !SamplePose(Output, Set->StandIdle, Layer.Time, true))
		{
			Output.ResetToRefPose();
		}
		break;
	}

	case EZLDeadlockPoseState::Slide:
		EvaluateIntoLoop(Output, Set->SlideStart, Set->SlideLoop, nullptr, 0.f, Layer.Time);
		break;

	case EZLDeadlockPoseState::Zipline:
		EvaluateIntoLoop(Output, Set->ZiplineStart, Set->ZiplineLoop, nullptr, 0.f, Layer.Time);
		break;
	}
}

// ------------------------------------------------------------------------------------------------ evaluate

void FAnimNode_ZLDeadlockHero::ApplyAimOffset(FPoseContext& Output) const
{
	if (AimWeight < 0.01f || Layers.Num() == 0)
	{
		return;
	}

	const EZLDeadlockPoseState Top = Layers.Last().State;
	const FZLAimAnims* Aim = &Set->StandAim;
	if (Top == EZLDeadlockPoseState::Crouch && Set->CrouchAim.IsValid())
	{
		Aim = &Set->CrouchAim;
	}
	else if (Top == EZLDeadlockPoseState::Ground && MoveAlpha > 0.5f && Set->RunAim.IsValid())
	{
		Aim = &Set->RunAim;
	}
	if (!Aim->IsValid())
	{
		return;
	}

	const float Pitch = FMath::Clamp(SmoothedAimPitch / Set->MaxAimPitch, -1.f, 1.f);
	const UAnimSequence* Target = Pitch >= 0.f ? Aim->Up.Get() : Aim->Down.Get();
	const float Weight = FMath::Abs(Pitch) * AimWeight;
	if (!Target || Weight < 0.01f)
	{
		return;
	}

	// Additive delta = Target - Center, applied only to the upper body.
	FPoseContext Center(Output);
	FPoseContext Delta(Output);
	SamplePose(Center, Aim->Center, 0.f, false);
	SamplePose(Delta, Target, 0.f, false);
	FAnimationRuntime::ConvertPoseToAdditive(Delta.Pose, Center.Pose);

	FPoseContext Aimed(Output);
	ZLDeadlockHero::CopyPose(Aimed, Output);
	FAnimationPoseData AimedData(Aimed);
	const FAnimationPoseData DeltaData(Delta);
	FAnimationRuntime::AccumulateAdditivePose(AimedData, DeltaData, Weight, AAT_LocalSpaceBase);
	Aimed.Pose.NormalizeRotations();

	FAnimationRuntime::LerpPosesPerBone(Output.Pose, Aimed.Pose, Output.Curve, Aimed.Curve, 1.f, AimBoneWeights);
}

void FAnimNode_ZLDeadlockHero::ApplySlots(FPoseContext& Output) const
{
	FAnimInstanceProxy* Proxy = Output.AnimInstanceProxy;

	if (!UpperBodySlot.IsNone() && UpperBodyWeights.SlotNodeWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		FPoseContext Source(Output);
		ZLDeadlockHero::CopyPose(Source, Output);
		FPoseContext UpperBody(Output);
		const FAnimationPoseData SourceData(Source);
		FAnimationPoseData UpperBodyData(UpperBody);
		Proxy->SlotEvaluatePose(UpperBodySlot, SourceData, UpperBodyWeights.SourceWeight, UpperBodyData, UpperBodyWeights.SlotNodeWeight, UpperBodyWeights.TotalNodeWeight);
		FAnimationRuntime::LerpPosesPerBone(Output.Pose, UpperBody.Pose, Output.Curve, UpperBody.Curve, 1.f, UpperBodyBoneWeights);
	}

	if (!FullBodySlot.IsNone() && FullBodyWeights.SlotNodeWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		FPoseContext Source(Output);
		ZLDeadlockHero::CopyPose(Source, Output);
		const FAnimationPoseData SourceData(Source);
		FAnimationPoseData OutputData(Output);
		Proxy->SlotEvaluatePose(FullBodySlot, SourceData, FullBodyWeights.SourceWeight, OutputData, FullBodyWeights.SlotNodeWeight, FullBodyWeights.TotalNodeWeight);
	}
}

void FAnimNode_ZLDeadlockHero::StripRootMotion(FPoseContext& Output) const
{
	if (!bStripRootMotion || !RootMotionIndex.IsValid() || RootMotionIndex.GetInt() >= Output.Pose.GetNumBones())
	{
		return;
	}

	// Deadlock clips move root_motion and its siblings (pelvis, IK targets) together. Shift them all back
	// by root_motion's offset from its reference pose so the body stays on the capsule.
	const FTransform& RefPose = Output.Pose.GetRefPose(RootMotionIndex);
	FVector Offset = Output.Pose[RootMotionIndex].GetTranslation() - RefPose.GetTranslation();
	if (!bStripVerticalRootMotion)
	{
		// "Vertical" is component-space Z; the shared parent may be rotated by the FBX axis conversion.
		const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(RootMotionIndex);
		const FQuat ParentRotation = Parent.IsValid() ? Output.Pose[Parent].GetRotation() : FQuat::Identity;
		FVector ComponentOffset = ParentRotation.RotateVector(Offset);
		ComponentOffset.Z = 0.f;
		Offset = ParentRotation.UnrotateVector(ComponentOffset);
	}

	for (const FCompactPoseBoneIndex& Bone : RootSiblings)
	{
		if (Bone.GetInt() < Output.Pose.GetNumBones())
		{
			Output.Pose[Bone].AddToTranslation(-Offset);
		}
	}
	Output.Pose[RootMotionIndex].SetTranslation(RefPose.GetTranslation());
}

void FAnimNode_ZLDeadlockHero::Evaluate_AnyThread(FPoseContext& Output)
{
	bool bEvaluated = false;
	if (Set && Layers.Num() > 0)
	{
		float AccumulatedWeight = 0.f;
		for (const FLayer& Layer : Layers)
		{
			if (Layer.Weight <= ZERO_ANIMWEIGHT_THRESH)
			{
				continue;
			}
			if (!bEvaluated)
			{
				EvaluateLayer(Layer, Output);
				AccumulatedWeight = Layer.Weight;
				bEvaluated = true;
				continue;
			}
			FPoseContext LayerPose(Output);
			EvaluateLayer(Layer, LayerPose);
			const float Alpha = Layer.Weight / (AccumulatedWeight + Layer.Weight);
			FAnimationRuntime::LerpPoses(Output.Pose, LayerPose.Pose, Output.Curve, LayerPose.Curve, Alpha);
			AccumulatedWeight += Layer.Weight;
		}
		if (bEvaluated)
		{
			ApplyAimOffset(Output);
		}
	}
	if (!bEvaluated)
	{
		Output.ResetToRefPose();
	}

	ApplySlots(Output);
	StripRootMotion(Output);
}

void FAnimNode_ZLDeadlockHero::GatherDebugData(FNodeDebugData& DebugData)
{
	FString Line = DebugData.GetNodeName(this);
	Line += FString::Printf(TEXT("(Set: %s, Speed: %.0f, MoveAlpha: %.2f, Aim: %.0f) Layers:"), *GetNameSafe(Set), State.GroundSpeed, MoveAlpha, SmoothedAimPitch);
	for (const FLayer& Layer : Layers)
	{
		Line += FString::Printf(TEXT(" %s=%.2f"), *UEnum::GetValueAsString(Layer.State), Layer.Weight);
	}
	DebugData.AddDebugItem(Line, true);
}
