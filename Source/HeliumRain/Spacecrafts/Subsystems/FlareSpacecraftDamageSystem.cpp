
#include "FlareSpacecraftDamageSystem.h"
#include "../../Flare.h"

#include "../FlareSpacecraft.h"
#include "../../Game/FlareGame.h"
#include "../../Player/FlarePlayerController.h"
#include "../FlareEngine.h"
#include "../FlareOrbitalEngine.h"
#include "../FlareShell.h"
#include "Engine/StaticMeshActor.h"

DECLARE_CYCLE_STAT(TEXT("FlareDamageSystem Tick"), STAT_FlareDamageSystem_Tick, STATGROUP_Flare);

#define LOCTEXT_NAMESPACE "FlareSpacecraftDamageSystem"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareSpacecraftDamageSystem::UFlareSpacecraftDamageSystem(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	, Spacecraft(NULL)
	, LastDamageCauser(NULL)
{
}


/*----------------------------------------------------
	Gameplay events
----------------------------------------------------*/

void UFlareSpacecraftDamageSystem::TickSystem(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_FlareDamageSystem_Tick);

	Parent->TickSystem();

	// Apply heat variation : add producted heat then substract radiated heat.

	// Get the to heat production and heat sink surface
	float HeatProduction = 0.f;
	float HeatSinkSurface = 0.f;

	for (int32 i = 0; i < Components.Num(); i++)
	{
		UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[i]);
		HeatProduction += Component->GetHeatProduction();
		HeatSinkSurface += Component->GetHeatSinkSurface();
	}

	// Add a part of sun radiation to ship heat production
	// Sun flow is 3.094KW/m^2 and keep only 10 % and modulate 90% by sun occlusion
	HeatProduction += HeatSinkSurface * 3.094 * 0.1 * (1 - 0.9 * Spacecraft->GetGame()->GetPlanetarium()->GetSunOcclusion());

	// Heat up
	Data->Heat += HeatProduction * DeltaSeconds;
	// Radiate: Stefan-Boltzmann constant=5.670373e-8
	float Temperature = Data->Heat / Description->HeatCapacity;
	float HeatRadiation = 0.f;
	if (Temperature > 0)
	{
		HeatRadiation = HeatSinkSurface * 5.670373e-8 * FMath::Pow(Temperature, 4) / 1000;
	}
	// Don't radiate too much energy : negative temperature is not possible
	Data->Heat -= FMath::Min(HeatRadiation * DeltaSeconds, Data->Heat);

	// Power outage
	if (Data->PowerOutageDelay > 0)
	{
		Data->PowerOutageDelay -=  DeltaSeconds;
		if (Data->PowerOutageDelay <=0)
		{
			Data->PowerOutageDelay = 0;
			UpdatePower(); // To update light
		}
	}

	// Update uncontrollable status
	if (WasControllable && Parent->IsUncontrollable())
	{
		AFlarePlayerController* PC = Spacecraft->GetGame()->GetPC();

		// Player kill
		if (PC && LastDamageCauser == PC->GetShipPawn() && Spacecraft != PC->GetShipPawn())
		{
			if(Parent->IsAlive())
			{
				PC->Notify(LOCTEXT("TargetShipUncontrollable", "Enemy disabled"),
					FText::Format(LOCTEXT("TargetShipUncontrollableFormat", "You rendered a ship uncontrollable ({0}-class)"), Spacecraft->GetParent()->GetDescription()->Name),
					FName("ship-uncontrollable"),
					EFlareNotification::NT_Info);
			}
		}

		// Company kill
		else if (PC && LastDamageCauser && PC->GetCompany() == LastDamageCauser->GetParent()->GetCompany())
		{
			if(Parent->IsAlive())
			{
				PC->Notify(LOCTEXT("TargetShipUncontrollableCompany", "Enemy disabled"),
					FText::Format(LOCTEXT("TargetShipUncontrollableCompanyFormat", "Your {0}-class ship rendered a ship uncontrollable ({1}-class)"),
						LastDamageCauser->GetParent()->GetDescription()->Name,
						Spacecraft->GetParent()->GetDescription()->Name),
					FName("ship-uncontrollable"),
					EFlareNotification::NT_Info);
			}
		}

		WasControllable = false;

		OnControlLost();
	}


	// Update alive status
	if (WasAlive && !Parent->IsAlive())
	{
		AFlarePlayerController* PC = Spacecraft->GetGame()->GetPC();

		// Player kill
		if (PC && LastDamageCauser == PC->GetShipPawn() && Spacecraft != PC->GetShipPawn())
		{
			PC->Notify(LOCTEXT("TargetShipKilled", "Enemy destroyed"),
				FText::Format(LOCTEXT("TargetShipKilledFormat", "You destroyed a ship ({0}-class)"), Spacecraft->GetParent()->GetDescription()->Name),
				FName("ship-killed"),
				EFlareNotification::NT_Info);
		}

		// Company kill
		else if (PC && LastDamageCauser && PC->GetCompany() == LastDamageCauser->GetParent()->GetCompany())
		{
			if(Spacecraft->IsStation())
			{
				PC->Notify(LOCTEXT("TargetStationKilledCompany", "Enemy destroyed"),
					FText::Format(LOCTEXT("TargetStationKilledCompanyFormat", "Your {0}-class ship destroyed a station ({1}-class)"),
						LastDamageCauser->GetParent()->GetDescription()->Name,
						Spacecraft->GetParent()->GetDescription()->Name),
					FName("station-killed"),
					EFlareNotification::NT_Info);
			}
			else
			{
				PC->Notify(LOCTEXT("TargetShipKilledCompany", "Enemy destroyed"),
					FText::Format(LOCTEXT("TargetShipKilledCompanyFormat", "Your {0}-class ship destroyed a ship ({1}-class)"),
						LastDamageCauser->GetParent()->GetDescription()->Name,
						Spacecraft->GetParent()->GetDescription()->Name),
					FName("ship-killed"),
					EFlareNotification::NT_Info);
			}
		}

		WasAlive = false;
		OnSpacecraftDestroyed();
	}

	TimeSinceLastExternalDamage += DeltaSeconds;
}

void UFlareSpacecraftDamageSystem::Initialize(AFlareSpacecraft* OwnerSpacecraft, FFlareSpacecraftSave* OwnerData)
{
	Spacecraft = OwnerSpacecraft;
	Components = Spacecraft->GetComponentsByClass(UFlareSpacecraftComponent::StaticClass());
	Description = Spacecraft->GetParent()->GetDescription();
	Data = OwnerData;
	Parent = Spacecraft->GetParent()->GetDamageSystem();
}

void UFlareSpacecraftDamageSystem::Start()
{
	// Reload components
	Components = Spacecraft->GetComponentsByClass(UFlareSpacecraftComponent::StaticClass());
	Parent->TickSystem();

	// Init alive status
	WasControllable = !Parent->IsUncontrollable();
	WasAlive = Parent->IsAlive();
	TimeSinceLastExternalDamage = 10000;

	AFlarePlayerController* PC = Spacecraft->GetGame()->GetPC();
	if (Spacecraft->GetParent()->GetCurrentFleet() == PC->GetPlayerFleet())
	{
		CheckRecovery();
	}
}

void UFlareSpacecraftDamageSystem::SetLastDamageCauser(AFlareSpacecraft* Ship)
{
	LastDamageCauser = Ship;
}

void UFlareSpacecraftDamageSystem::UpdatePower()
{
	for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[ComponentIndex]);
		Component->UpdateLight();
	}
}

void UFlareSpacecraftDamageSystem::OnSpacecraftDestroyed()
{
	AFlarePlayerController* PC = Spacecraft->GetGame()->GetPC();

	// Lost player ship
	if (Spacecraft == PC->GetShipPawn())
	{
		if (LastDamageCauser)
		{
			if(LastDamageCauser->IsStation())
			{
				PC->Notify(
					LOCTEXT("PlayerShipDestroyedByStation", "Your ship has been destroyed"),
					FText::Format(LOCTEXT("ShipDestroyedFormat", "Your ship was destroyed by a {0}-class station"), LastDamageCauser->GetParent()->GetDescription()->Name),
					FName("ship-destroyed"),
					EFlareNotification::NT_Military,
					false,
					EFlareMenu::MENU_Company);
			}
			else
			{
				PC->Notify(
					LOCTEXT("PlayerShipDestroyedByShip", "Your ship has been destroyed"),
					FText::Format(LOCTEXT("ShipDestroyedFormat", "Your ship was destroyed by a {0}-class ship"), LastDamageCauser->GetParent()->GetDescription()->Name),
					FName("ship-destroyed"),
					EFlareNotification::NT_Military,
					false,
					EFlareMenu::MENU_Company);
			}

		}
		else
		{
			PC->Notify(
				LOCTEXT("PlayerShipCrashedDestroyed", "Crash"),
				FText::FText(LOCTEXT("ShipCrashedDestroyedFormat", "You crashed your ship")),
				FName("ship-crashed"),
				EFlareNotification::NT_Military,
				false,
				EFlareMenu::MENU_Company);
		}
	}

	// Lost company ship
	else if (Spacecraft->GetParent()->GetCompany() == PC->GetCompany())
	{
		if(!Spacecraft->IsStation())
		{
			PC->Notify(LOCTEXT("ShipDestroyedCompany", "You lost a ship"),
				FText::Format(LOCTEXT("ShipDestroyedCompanyFormat", "Your {0}-class ship was destroyed"), Spacecraft->GetParent()->GetDescription()->Name),
				FName("ship-killed"),
				EFlareNotification::NT_Military);
		}
		else
		{
			PC->Notify(LOCTEXT("StationDestroyedCompany", "You lost a station"),
				FText::Format(LOCTEXT("StationDestroyedCompanyFormat", "Your {0}-class station was destroyed"), Spacecraft->GetParent()->GetDescription()->Name),
				FName("station-killed"),
				EFlareNotification::NT_Military);
		}
	}

	Spacecraft->GetGame()->GetQuestManager()->OnSpacecraftDestroyed(Spacecraft->GetParent(), false,
																	LastDamageCauser ? LastDamageCauser->GetCompany() : NULL);

	// This ship has been damaged and someone is to blame
	if (LastDamageCauser != NULL && LastDamageCauser->GetCompany() != Spacecraft->GetCompany())
	{
		// If it's a betrayal, lower attacker's reputation on everyone, give rep to victim
		if (Spacecraft->GetCompany()->GetWarState(LastDamageCauser->GetCompany()) != EFlareHostility::Hostile)
		{
			float ReputationCost = -40;

			Spacecraft->GetCompany()->GiveReputationToOthers(-0.3 * ReputationCost, false);
			LastDamageCauser->GetCompany()->GiveReputationToOthers(0.5*ReputationCost, false);

			// Lower attacker's reputation on victim
			Spacecraft->GetCompany()->GiveReputation(LastDamageCauser->GetCompany(), ReputationCost, true);
			if(LastDamageCauser->GetCompany() == PC->GetCompany())
			{
				for(UFlareCompany* Company : PC->GetGame()->GetGameWorld()->GetCompanies())
				{
					if(Company->GetReputation(PC->GetCompany()) < -100)
					{
						// Consider player just declare war
						Company->SetHostilityTo(PC->GetCompany(), true);
						PC->GetCompany()->SetHostilityTo(Company, true);
					}
				}
			}
		}
	}
}

void UFlareSpacecraftDamageSystem::OnControlLost()
{
	AFlarePlayerController* PC = Spacecraft->GetGame()->GetPC();

	// Lost player ship
	if (Spacecraft == PC->GetShipPawn())
	{
		if (LastDamageCauser)
		{
			if (Parent->IsAlive())
			{
				PC->Notify(
					LOCTEXT("PlayerShipUncontrollable", "Your ship is uncontrollable"),
					FText::Format(LOCTEXT("ShipUncontrollableFormat", "Your ship has been rendered uncontrollable by a {0}-class ship"), LastDamageCauser->GetParent()->GetDescription()->Name),
					FName("ship-uncontrollable"),
					EFlareNotification::NT_Military,
					false,
					EFlareMenu::MENU_Company);
			}
		}
		else
		{
			if (Parent->IsAlive())
			{
				PC->Notify(
					LOCTEXT("PlayerShipUncontrollableCrashed", "Crash"),
					FText::FText(LOCTEXT("ShipUncontrollableCrashedFormat", "You crashed your ship")),
					FName("ship-uncontrollable-crashed"),
					EFlareNotification::NT_Military,
					false,
					EFlareMenu::MENU_Company);
			}
		}
	}

	// Lost company ship
	else if (Spacecraft->GetParent()->GetCompany() == PC->GetCompany())
	{
		if (Parent->IsAlive() && Spacecraft->IsStation())
		{
		PC->Notify(LOCTEXT("ShipUncontrollableCompany", "Ship uncontrollable"),
			FText::Format(LOCTEXT("ShipUncontrollableCompanyFormat", "Your {0}-class ship is uncontrollable"), Spacecraft->GetParent()->GetDescription()->Name),
			FName("ship-uncontrollable"),
			EFlareNotification::NT_Military);
		}
	}

	if (Spacecraft->GetParent()->GetCurrentFleet() == PC->GetPlayerFleet())
	{
		CheckRecovery();
	}

	Spacecraft->GetNavigationSystem()->Undock();
	Spacecraft->GetNavigationSystem()->AbortAllCommands();

	Spacecraft->GetGame()->GetQuestManager()->OnSpacecraftDestroyed(Spacecraft->GetParent(), true,
																	LastDamageCauser ? LastDamageCauser->GetCompany() : NULL);


	// This ship has been damaged and someone is to blame
	if (LastDamageCauser != NULL && LastDamageCauser->GetCompany() != Spacecraft->GetCompany())
	{
		// If it's a betrayal, lower attacker's reputation on everyone, give rep to victim
		if (Spacecraft->GetCompany()->GetWarState(LastDamageCauser->GetCompany()) != EFlareHostility::Hostile)
		{
			float ReputationCost = -30;

			Spacecraft->GetCompany()->GiveReputationToOthers(-0.2 * ReputationCost, false);
			LastDamageCauser->GetCompany()->GiveReputationToOthers(0.5*ReputationCost, false);

			// Lower attacker's reputation on victim
			Spacecraft->GetCompany()->GiveReputation(LastDamageCauser->GetCompany(), ReputationCost, true);
			if(LastDamageCauser->GetCompany() == PC->GetCompany())
			{
				for(UFlareCompany* Company : PC->GetGame()->GetGameWorld()->GetCompanies())
				{
					if(Company->GetReputation(PC->GetCompany()) < -100)
					{
						// Consider player just declare war
						Company->SetHostilityTo(PC->GetCompany(), true);
						PC->GetCompany()->SetHostilityTo(Company, true);
					}
				}
			}
		}
	}
}

void UFlareSpacecraftDamageSystem::CheckRecovery()
{
	AFlarePlayerController* PC = Spacecraft->GetGame()->GetPC();

	// Check if it the last ship
	bool EmptyFleet = true;
	for(int ShipIndex = 0; ShipIndex < PC->GetPlayerFleet()->GetShips().Num(); ShipIndex++)
	{
		UFlareSimulatedSpacecraft* Ship = PC->GetPlayerFleet()->GetShips()[ShipIndex];
		if(Ship->GetDamageSystem()->IsAlive() && !Ship->GetDamageSystem()->IsUncontrollable())
		{
			EmptyFleet = false;
			break;
		}
	}

	// If last, activate recovery
	if (EmptyFleet)
	{
		PC->GetMenuManager()->OpenMenu(EFlareMenu::MENU_GameOver);
	}
}

void UFlareSpacecraftDamageSystem::OnCollision(class AActor* Other, FVector HitLocation, FVector NormalImpulse)
{
	// If receive hit from over actor, like a ship we must apply collision damages.
	// The applied damage energy is 0.2% of the kinetic energy of the other actor. The kinetic
	// energy is calculated from the relative speed between the 2 actors, and only with the relative
	// speed projected in the axis of the collision normal: if 2 very fast ship only slightly touch,
	// only few energy will be decipated by the impact.
	//
	// The damages are applied only to the current actor, the ReceiveHit method of the other actor
	// will also call an it will apply its collision damages itself.

	// If the other actor is a projectile, specific weapon damage code is done in the projectile hit
	// handler: in this case we ignore the collision
	AFlareShell* OtherProjectile = Cast<AFlareShell>(Other);
	if (OtherProjectile)
	{
		return;
	}

	AFlareBomb* OtherBomb = Cast<AFlareBomb>(Other);
	if (OtherBomb)
	{
		return;
	}
	// No primitive component, ignore
	UPrimitiveComponent* OtherRoot = Cast<UPrimitiveComponent>(Other->GetRootComponent());
	if (!OtherRoot)
	{
		return;
	}

	// Ignore debris
	AStaticMeshActor* OtherActor = Cast<AStaticMeshActor>(Other);
	if (OtherActor)
	{
		if (OtherActor->GetName().StartsWith("Debris"))
		{
			return;
		}
	}

	// Ignore docking or docked station
	AFlareSpacecraft* OtherSpacecraft = Cast<AFlareSpacecraft>(Other);

	if(OtherSpacecraft && (
			Spacecraft->GetDockingSystem()->IsDockedShip(OtherSpacecraft) ||
			OtherSpacecraft->GetDockingSystem()->IsDockedShip(Spacecraft) ||
			Spacecraft->GetDockingSystem()->IsGrantedShip(OtherSpacecraft) ||
			OtherSpacecraft->GetDockingSystem()->IsGrantedShip(Spacecraft)))
	{
		// #490 : disable this warning until we fix the actual bug some day...
		/*FLOGV("WARNING: Avoid unexpected damage between docked or docking spacecraft : %s and %s",
			  *Spacecraft->GetImmatriculation().ToString(),
			  *OtherSpacecraft->GetImmatriculation().ToString());*/
		return;
	}

	// Relative velocity
	FVector DeltaVelocity = ((OtherRoot->GetPhysicsLinearVelocity() - Spacecraft->Airframe->GetPhysicsLinearVelocity()) / 100);

	// Compute the relative velocity in the impact axis then compute kinetic energy
	/*float ImpactSpeed = DeltaVelocity.Size();
	float ImpactMass = FMath::Min(Spacecraft->GetSpacecraftMass(), OtherRoot->GetMass());
	*/

	//200 m /s -> 6301.873047 * 20000 -> 300 / 2 damage

	float ImpactSpeed = 0;
	float ImpactEnergy = 0;
	float ImpactMass = Spacecraft->GetSpacecraftMass();

	// Check if the mass was set and is valid
	if (ImpactMass > KINDA_SMALL_NUMBER)
	{
		ImpactSpeed = NormalImpulse.Size() / (ImpactMass * 100.f);
		ImpactEnergy = ImpactMass * ImpactSpeed / 8402.f;
	}

	float  Radius = 0.2 + FMath::Sqrt(ImpactEnergy) * 0.11;
	//FLOGV("OnCollision %s", *Spacecraft->GetImmatriculation().ToString());
	//FLOGV("  OtherRoot->GetPhysicsLinearVelocity()=%s", *OtherRoot->GetPhysicsLinearVelocity().ToString());
	//FLOGV("  OtherRoot->GetPhysicsLinearVelocity().Size()=%f", OtherRoot->GetPhysicsLinearVelocity().Size());
	//FLOGV("  Spacecraft->Airframe->GetPhysicsLinearVelocity()=%s", *Spacecraft->Airframe->GetPhysicsLinearVelocity().ToString());
	//FLOGV("  Spacecraft->Airframe->GetPhysicsLinearVelocity().Size()=%f", Spacecraft->Airframe->GetPhysicsLinearVelocity().Size());
	//FLOGV("  dot=%f", FVector::DotProduct(DeltaVelocity.GetUnsafeNormal(), HitNormal.GetUnsafeNormal()));
	/*FLOGV("  DeltaVelocity=%s", *DeltaVelocity.ToString());
	FLOGV("  ImpactSpeed=%f", ImpactSpeed);
	FLOGV("  ImpactMass=%f", ImpactMass);
	FLOGV("  ImpactEnergy=%f", ImpactEnergy);
	FLOGV("  Radius=%f", Radius);*/



	bool HasHit = false;
	FHitResult BestHitResult;
	float BestHitDistance = 0;

	for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[ComponentIndex]);
		if (Component)
		{
			FHitResult HitResult(ForceInit);
			FCollisionQueryParams TraceParams(FName(TEXT("Fragment Trace")), true);
			TraceParams.bTraceComplex = true;
			TraceParams.bReturnPhysicalMaterial = false;
			Component->LineTraceComponent(HitResult, HitLocation, HitLocation + Spacecraft->GetLinearVelocity().GetUnsafeNormal() * 10000, TraceParams);

			if (HitResult.Actor.IsValid()){
				float HitDistance = (HitResult.Location - HitLocation).Size();
				if (!HasHit || HitDistance < BestHitDistance)
				{
					BestHitDistance = HitDistance;
					BestHitResult = HitResult;
				}

				//FLOGV("Collide hit %s at a distance=%f", *Component->GetReadableName(), HitDistance);
				HasHit = true;
			}
		}

	}

	if (HasHit)
	{
		//DrawDebugLine(Spacecraft->GetWorld(), HitLocation, BestHitResult.Location, FColor::Magenta, true);
	}
	else
	{
		int32 BestComponentIndex = -1;
		BestHitDistance = 0;

		for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
		{
			UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[ComponentIndex]);
			if (Component)
			{
				float ComponentDistance = (Component->GetComponentLocation() - HitLocation).Size();
				if (BestComponentIndex == -1 || BestHitDistance > ComponentDistance)
				{
					BestComponentIndex = ComponentIndex;
					BestHitDistance = ComponentDistance;
				}
			}
		}
		UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[BestComponentIndex]);


		FCollisionQueryParams TraceParams(FName(TEXT("Fragment Trace")), true);
		TraceParams.bTraceComplex = true;
		TraceParams.bReturnPhysicalMaterial = false;
		Component->LineTraceComponent(BestHitResult, HitLocation, Component->GetComponentLocation(), TraceParams);
		//DrawDebugLine(Spacecraft->GetWorld(), HitLocation, BestHitResult.Location, FColor::Yellow, true);


	}

	UFlareSimulatedSpacecraft* DamageSource = NULL;
	if (OtherSpacecraft)
	{
		DamageSource = OtherSpacecraft->GetParent();
		LastDamageCauser = OtherSpacecraft;
	}
	else
	{
		LastDamageCauser = NULL;
	}

	FString DamageSourceName = OtherSpacecraft ? OtherSpacecraft->GetImmatriculation().ToString() : Other->GetFullName();
	ApplyDamage(ImpactEnergy, Radius, BestHitResult.Location, EFlareDamage::DAM_Collision, DamageSource, DamageSourceName);
}

void UFlareSpacecraftDamageSystem::ApplyDamage(float Energy, float Radius, FVector Location, EFlareDamage::Type DamageType, UFlareSimulatedSpacecraft* DamageSource, FString DamageCauser)
{
	// The damages are applied to all component touching the sphere defined by the radius and the
	// location in parameter.
	// The maximum damage are applied to a component only if its bounding sphere touch the center of
	// the damage sphere. There is a linear decrease of damage with a minumum of 0 if the 2 sphere
	// only touch.

	//FLOGV("Apply %f damages to %s with radius %f at %s", Energy, *(Spacecraft->GetImmatriculation().ToString()), Radius, *Location.ToString());
	//DrawDebugSphere(Spacecraft->GetWorld(), Location, Radius * 100, 12, FColor::Red, true);

	// Signal the player he's hit something
	AFlarePlayerController* PC = Spacecraft->GetGame()->GetPC();
	if (DamageSource == PC->GetShipPawn()->GetParent())
	{
		PC->SignalHit(Spacecraft, DamageType);
	}

	// Signal the player he's been damaged
	if (Spacecraft == PC->GetShipPawn())
	{
		switch (DamageType)
		{
			case EFlareDamage::DAM_ArmorPiercing:
			case EFlareDamage::DAM_HighExplosive:
			case EFlareDamage::DAM_HEAT:
				PC->SpacecraftHit(DamageSource->GetDescription()->Size);
				break;
			case EFlareDamage::DAM_Collision:
				PC->SpacecraftCrashed();
				break;
			case EFlareDamage::DAM_Overheat:
			default:
				break;
		}
	}

	UFlareCompany* CompanyDamageSource = (DamageSource ? DamageSource->GetCompany() : NULL);

	FVector LocalLocation = Spacecraft->GetRootComponent()->GetComponentTransform().InverseTransformPosition(Location) / 100.f;
	CombatLog::SpacecraftDamaged(Spacecraft->GetParent(), Energy, Radius, LocalLocation, DamageType, CompanyDamageSource, DamageCauser);

#if! UE_BUILD_SHIPPING
	
	// Stations damaged by collisions with non-spacecrafts are highly suspicious
	if (Spacecraft->IsStation() && DamageType == EFlareDamage::DAM_Collision)
	{
		FCHECK(DamageSource);
		FCHECK(CompanyDamageSource);
	}

#endif

	for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[ComponentIndex]);
		bool IsStationCockpit = Spacecraft->IsStation() && Spacecraft->GetCockpit() == Component;

		float ComponentSize;
		FVector ComponentLocation;
		Component->GetBoundingSphere(ComponentLocation, ComponentSize);

		float Distance = (ComponentLocation - Location).Size() / 100.0f;
		float IntersectDistance =  Radius + ComponentSize/100 - Distance;

		//DrawDebugSphere(Spacecraft->GetWorld(), ComponentLocation, ComponentSize, 12, FColor::Green, true);

		// Hit this component
		if (IntersectDistance > 0 || IsStationCockpit)
		{
			//FLOGV("Component %s. ComponentSize=%f, Distance=%f, IntersectDistance=%f", *(Component->GetReadableName()), ComponentSize, Distance, IntersectDistance);
			float Efficiency = FMath::Clamp(IntersectDistance / Radius , 0.0f, 1.0f);
			if(IsStationCockpit)
			{
				Efficiency = 1;
			}
			float InflictedDamageRatio = Component->ApplyDamage(Energy * Efficiency, DamageType, CompanyDamageSource);
		}
	}

	// Update power
	UpdatePower();

	// Heat the ship
	Data->Heat += Energy;

	switch (DamageType)
	{
		case EFlareDamage::DAM_ArmorPiercing:
		case EFlareDamage::DAM_HighExplosive:
		case EFlareDamage::DAM_HEAT:
			//FLOGV("%s Reset TimeSinceLastExternalDamage", *Spacecraft->GetImmatriculation().ToString());
			TimeSinceLastExternalDamage = 0;
			break;
		case EFlareDamage::DAM_Collision:
		case EFlareDamage::DAM_Overheat:
		default:
			// Don't reset timer
			break;
	}
}

void UFlareSpacecraftDamageSystem::OnElectricDamage(float DamageRatio)
{
	float MaxPower = 0.f;
	float AvailablePower = 0.f;

	float Total = 0.f;
	float GeneratorCount = 0;
	for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		UFlareSpacecraftComponent* Component = Cast<UFlareSpacecraftComponent>(Components[ComponentIndex]);
		MaxPower += Component->GetMaxGeneratedPower();
		AvailablePower += Component->GetGeneratedPower();
	}

	float PowerRatio = AvailablePower/MaxPower;


	//FLOGV("OnElectricDamage initial PowerOutageDelay=%f, PowerOutageAcculumator=%f, DamageRatio=%f, PowerRatio=%f", Data->PowerOutageDelay, Data->PowerOutageAcculumator, DamageRatio, PowerRatio);


	Data->PowerOutageAcculumator += DamageRatio * 2.f;
	if (!Data->PowerOutageDelay && PowerRatio > 0)
	{
		if (FMath::FRand() > 1.f + PowerRatio / 2.f - Data->PowerOutageAcculumator / 2.f)
		{
			Data->PowerOutageDelay += FMath::FRandRange(1, 5) /  (2 * PowerRatio);
			Data->PowerOutageAcculumator = -Data->PowerOutageAcculumator * PowerRatio;
			UpdatePower();
		}
	}
}

float UFlareSpacecraftDamageSystem::GetWeaponGroupHealth(int32 GroupIndex, bool WithAmmo) const
{
	 FFlareWeaponGroup* WeaponGroup = Spacecraft->GetWeaponsSystem()->GetWeaponGroup(GroupIndex);
	 float Health = 0.0;

	 float Total = 0.f;
	 for (int32 ComponentIndex = 0; ComponentIndex < WeaponGroup->Weapons.Num(); ComponentIndex++)
	 {
		 UFlareWeapon* Weapon = Cast<UFlareWeapon>(WeaponGroup->Weapons[ComponentIndex]);
		 Total += Weapon->GetUsableRatio()*((Weapon->GetCurrentAmmo() > 0 || !WithAmmo) ? 1 : 0);
	 }
	 Health = Total/WeaponGroup->Weapons.Num();

	 return Health;
}

float UFlareSpacecraftDamageSystem::GetOverheatRatio(float HalfRatio) const
{
	if (Parent->GetTemperature() <= Parent->GetOverheatTemperature())
	{
		return 0;
	}
	else
	{
		float OverHeat = Parent->GetTemperature() - Parent->GetOverheatTemperature();
		float BaseOverHeatRatio = OverHeat / Parent->GetOverheatTemperature();


		return (-1/(BaseOverHeatRatio/HalfRatio+1))+1;
	}
}

#undef LOCTEXT_NAMESPACE
