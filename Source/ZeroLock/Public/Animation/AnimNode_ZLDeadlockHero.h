// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimTypes.h"
#include "Animation/ZL_DeadlockAnimInstance.h"
#include "AnimNode_ZLDeadlockHero.generated.h"

class UAnimSequence;
class UZL_DeadlockAnimSet;

UENUM()
enum class EZLDeadlockPoseState : uint8
{
	Ground,
	Crouch,
	Air,
	Land,
	Slide,
	Dash,
	WallJump,
	Zipline,
};

/**
 * The whole Deadlock hero pose in one node, driven by UZL_DeadlockAnimInstance + a UZL_DeadlockAnimSet:
 *  - 8-way idle/run and crouch cycles, phase-synced and speed-scaled
 *  - jump / multi-jump / apex / fall / land, slide, dash, wall jump, zipline, crossfaded between states
 *  - aim offset from the camera pitch
 *  - montage slots: an upper-body slot layered from UpperBodyRootBone, then a full-body slot
 *  - strips the root_motion bone's translation so every clip (montages included) plays in place; Mover moves the pawn
 * Works on any skeleton, so ABP_DeadlockHero can be a template anim blueprint shared by all heroes.
 */
USTRUCT(BlueprintInternalUseOnly)
struct ZEROLOCK_API FAnimNode_ZLDeadlockHero : public FAnimNode_Base
{
	GENERATED_BODY()

	/** Clips to use instead of the anim instance's set. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault))
	TObjectPtr<UZL_DeadlockAnimSet> AnimSetOverride;

	/** Montages in this slot only drive bones from UpperBodyRootBone up; the legs keep running. */
	UPROPERTY(EditAnywhere, Category = "Slots")
	FName UpperBodySlot = TEXT("UpperBody");

	/** Montages in this slot drive the whole body. */
	UPROPERTY(EditAnywhere, Category = "Slots")
	FName FullBodySlot = TEXT("DefaultSlot");

	UPROPERTY(EditAnywhere, Category = "Bones")
	FName UpperBodyRootBone = TEXT("spine_1");

	/** Aim offset only affects this bone and its children. */
	UPROPERTY(EditAnywhere, Category = "Bones")
	FName AimRootBone = TEXT("spine_0");

	UPROPERTY(EditAnywhere, Category = "Bones")
	FName RootMotionBone = TEXT("root_motion");

	/** Remove the root_motion bone's movement from the pose (and from bones that share its parent). */
	UPROPERTY(EditAnywhere, Category = "Bones")
	bool bStripRootMotion = true;

	/** Also strip vertical root motion (jump clips lift the whole body; Mover already does that). */
	UPROPERTY(EditAnywhere, Category = "Bones")
	bool bStripVerticalRootMotion = true;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

private:
	struct FLayer
	{
		EZLDeadlockPoseState State = EZLDeadlockPoseState::Ground;
		float Weight = 0.f;
		float BlendInTime = 0.2f;
		float Time = 0.f;
		/** Clip picked when the layer started (dash direction, land vs hard land, jump vs air jump). */
		TObjectPtr<UAnimSequence> Clip = nullptr;
	};

	struct FSlotWeights
	{
		float SlotNodeWeight = 0.f;
		float SourceWeight = 1.f;
		float TotalNodeWeight = 0.f;
	};

	EZLDeadlockPoseState ChooseState() const;
	/** StartTime: where the layer's clips begin, e.g. partway into SlideStart. */
	void PushLayer(EZLDeadlockPoseState NewState, float BlendTime, UAnimSequence* Clip = nullptr, float StartTime = 0.f);
	void UpdateLayerWeights(float DeltaTime);
	void UpdateLocomotion(float DeltaTime);
	float GetLayerHoldTime(const FLayer& Layer) const;

	void EvaluateLayer(const FLayer& Layer, FPoseContext& Output) const;
	void EvaluateLocomotion(FPoseContext& Output, UAnimSequence* Idle, const struct FZLDirectionalAnims& Cycle, float IdleTime) const;
	void EvaluateIntoLoop(FPoseContext& Output, UAnimSequence* Intro, UAnimSequence* LoopA, UAnimSequence* LoopB, float LoopBAlpha, float Time) const;
	void ApplyAimOffset(FPoseContext& Output) const;
	void ApplySlots(FPoseContext& Output) const;
	void StripRootMotion(FPoseContext& Output) const;

	static bool SamplePose(FPoseContext& Output, const UAnimSequence* Seq, float Time, bool bLoop);
	static void SampleBlend(FPoseContext& Output, const UAnimSequence* A, float TimeA, bool bLoopA, const UAnimSequence* B, float TimeB, bool bLoopB, float BAlpha);

	// Inputs copied from the anim instance each update
	TObjectPtr<UZL_DeadlockAnimSet> Set = nullptr;
	FZLDeadlockAnimState State;
	float DeltaTime = 0.f;

	TArray<FLayer> Layers;
	int32 SeenJumpCount = 0;
	int32 SeenLandCount = 0;
	int32 SeenDashCount = 0;

	/** Seconds since the character was last airborne. Mover can pass through Walking for a tick between Falling and Sliding. */
	float TimeSinceAirborne = 10.f;

	UAnimSequence* ChooseDashClip() const;

	float Phase = 0.f;
	float MoveAlpha = 0.f;
	FVector2D MoveDir = FVector2D(1.f, 0.f);
	float SmoothedAimPitch = 0.f;
	float AimWeight = 0.f;

	FSlotWeights UpperBodyWeights;
	FSlotWeights FullBodyWeights;
	FGraphTraversalCounter SlotNodeInitializationCounter;

	// Bone caches (compact pose indices)
	TArray<float> UpperBodyBoneWeights;
	TArray<float> AimBoneWeights;
	TArray<FCompactPoseBoneIndex> RootSiblings;
	FCompactPoseBoneIndex RootMotionIndex = FCompactPoseBoneIndex(INDEX_NONE);
};
