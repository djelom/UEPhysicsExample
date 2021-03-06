// Fill out your copyright notice in the Description page of Project Settings.

#include "UEPhysicsExample.h"
#include "MyCubeActor.h"
#include "MyStaticMeshComponent.h"

#include "PhysXIncludes.h"
#include "PhysicsPublic.h"
#include "Runtime/Engine/Private/PhysicsEngine/PhysXSupport.h"

void FMySecondaryTickFunction::ExecuteTick(
	float DeltaTime,
	ELevelTick TickType,
	ENamedThreads::Type CurrentThread,
	const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Target && !Target->IsPendingKill() && !Target->IsUnreachable())
	{
		FScopeCycleCounterUObject ActorScope(Target);
		Target->TickPostPhysics(DeltaTime /* * Target->CustomTimeDilation*/, TickType, *this);
	}
}

FString FMySecondaryTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[TickActor2]");
}

UMyStaticMeshComponent::UMyStaticMeshComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SecondaryComponentTick.TickGroup = TG_PostPhysics;
	SecondaryComponentTick.bCanEverTick = true;
	SecondaryComponentTick.bStartWithTickEnabled = true;

	OnCalculateCustomPhysics.BindUObject(this, &UMyStaticMeshComponent::SubstepTick);

	FrameCount = 0;
}

void UMyStaticMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	owner = Cast<AMyCubeActor>(GetOwner());

	if (!IsTemplate() && SecondaryComponentTick.bCanEverTick)
	{
		SecondaryComponentTick.Target = this;
		SecondaryComponentTick.SetTickFunctionEnable(SecondaryComponentTick.bStartWithTickEnabled);
		SecondaryComponentTick.RegisterTickFunction(owner->GetLevel());
	}

	PActorHandle = &GetBodyInstance()->GetPhysicsActorHandle();

	SetSimulatePhysics(true);
	SetEnableGravity(false);
	SetMassOverrideInKg();
	SetAngularDamping(0);
	SetLinearDamping(0);

	SetConstraintMode(EDOFMode::SixDOF);

	FBodyInstance *bi = GetBodyInstance();
	if (bi) {
		bi->bLockXTranslation = true;
		bi->bLockYTranslation = true;
		bi->bLockZTranslation = false;

		bi->bLockXRotation = true;
		bi->bLockYRotation = true;
		bi->bLockZRotation = true;
	}

	StartH = GetComponentLocation().Z;
	SetPhysicsLinearVelocity(FVector(0.0f, 0.0f, owner->StartVelocity));
}

void UMyStaticMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FrameCount++;

	if (owner->bEnableLogging)
		UE_LOG(LogClass, Log, TEXT("%d UMyStaticMeshComponent::TickComponent DeltaTime : %f, Z : %f"), FrameCount, DeltaTime, GetCurrentLocation().Z);

	if (owner->bSubstepEnabled)
	{
		// Add custom physics forces each tick
		GetBodyInstance()->AddCustomPhysics(OnCalculateCustomPhysics);
	}
	else DoPhysics(DeltaTime, false);
}

void UMyStaticMeshComponent::SubstepTick(float DeltaTime, FBodyInstance* InBodyInstance)
{
	if(owner->bEnableLogging)
		UE_LOG(LogClass, Log, TEXT("%d UMyStaticMeshComponent::SubstepTick DeltaTime : %f, Z : %f"), FrameCount, DeltaTime, GetCurrentLocation().Z);

	DoPhysics(DeltaTime, true);
}

void UMyStaticMeshComponent::DoPhysics(float DeltaTime, bool InSubstep)
{
	float CurrError = GetCurrentLocation().Z - StartH;

	float Velocity = GetCurrentVelocity().Z;

	float force = -(CurrError * owner->KSpring + Velocity * owner->Damping);

	if (InSubstep) {
		FPhysicsInterface::AddForce_AssumesLocked(*PActorHandle, FVector(0.0f, 0.f, force));
	}
	else {
		AddForce(FVector(0.0f, 0.0f, force));
	}
}

void UMyStaticMeshComponent::TickPostPhysics(
	float DeltaSeconds,
	ELevelTick TickType,
	FMySecondaryTickFunction& ThisTickFunction
	)
{
	// Non-player update.
	const bool bShouldTick =
		((TickType != LEVELTICK_ViewportsOnly) || owner->ShouldTickIfViewportsOnly());
	if (bShouldTick)
	{
		if (!IsPendingKill() && GetWorld())
		{
			if (owner->GetWorldSettings() != NULL && !IsRunningDedicatedServer())
			{
				// Here your post physics tick stuff
				if (owner->bEnableLogging)
					UE_LOG(LogClass, Log, TEXT("%d UMyStaticMeshComponent::TickPostPhysics DeltaTime: %f, Z: %f"), FrameCount, DeltaSeconds, GetCurrentLocation().Z);
			}
		}
	}
}

FVector UMyStaticMeshComponent::GetCurrentLocation() {
	PxTransform PTransform = PActorHandle->SyncActor->getGlobalPose();
	return FVector(PTransform.p.x, PTransform.p.y, PTransform.p.z);
}

FRotator UMyStaticMeshComponent::GetCurrentRotation() {
	PxTransform PTransform = PActorHandle->SyncActor->getGlobalPose();
	return FRotator(FQuat(PTransform.q.x, PTransform.q.y, PTransform.q.z, PTransform.q.w));
}

FVector UMyStaticMeshComponent::GetCurrentAngularVelocity() {
	FVector PAngVelocity = FPhysicsInterface::GetAngularVelocity_AssumesLocked(*PActorHandle);
	return FMath::RadiansToDegrees(PAngVelocity);
}

FVector UMyStaticMeshComponent::GetCurrentVelocity() {
	return FPhysicsInterface::GetLinearVelocity_AssumesLocked(*PActorHandle);
}