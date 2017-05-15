
#include "FlareAIBehavior.h"
#include "../../Flare.h"

#include "../FlareGame.h"
#include "../FlareCompany.h"
#include "../FlareScenarioTools.h"
#include "../../Quests/FlareQuest.h"
#include "../../Player/FlarePlayerController.h"
#include "../../Spacecrafts/FlareSimulatedSpacecraft.h"


//#define DEBUG_AI_NO_WAR

/*----------------------------------------------------
	Public API
----------------------------------------------------*/

UFlareAIBehavior::UFlareAIBehavior(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFlareAIBehavior::Load(UFlareCompany* ParentCompany)
{
	if(!Company)
	{
		Company = ParentCompany;
		check(Company);
		Game = Company->GetGame();
		ST = Game->GetScenarioTools();
		check(ST);

		GenerateAffilities();

		ProposeTributeToPlayer = false;
	}
}

inline static bool CompanyValueComparator(const UFlareCompany& ip1, const UFlareCompany& ip2)
{
	return ip1.GetCompanyValue().TotalValue < ip2.GetCompanyValue().TotalValue;
}


void UFlareAIBehavior::Simulate()
{
	TArray<UFlareCompany*> SortedCompany = Game->GetGameWorld()->GetCompanies();
	SortedCompany.Sort(&CompanyValueComparator);
	int32 AICompanyIndex = SortedCompany.IndexOfByKey(Company);

	// Reputation changes
	for (int32 CompanyIndex = 0; CompanyIndex < SortedCompany.Num(); CompanyIndex++)
	{
		UFlareCompany* OtherCompany = SortedCompany[CompanyIndex];

		if (OtherCompany == Company)
		{
			continue;
		}

		int IndexDelta = FMath::Abs(CompanyIndex - AICompanyIndex);


		int64 OtherCompanyValue = OtherCompany->GetCompanyValue().TotalValue;
		int64 CompanyValue = Company->GetCompanyValue().TotalValue;
		if(CompanyValue > OtherCompanyValue)
		{
			// Like more the most distant opponent
			float Intensity = IndexDelta * (1.f / SortedCompany.Num());
			Company->GiveReputation(OtherCompany, Intensity, false);
		}
		else
		{
			// Dislike more the closest opponent
			float Intensity = -1.f / IndexDelta;
			Company->GiveReputation(OtherCompany, Intensity, false);
		}

		if(Company == ST->Pirates)
		{
			Company->GiveReputation(OtherCompany, -1, false);
		}
	}


	if(Company == ST->Pirates)
	{
		SimulatePirateBehavior();
	}
	else
	{
		SimulateGeneralBehavior();
	}

}

void UFlareAIBehavior::SimulateGeneralBehavior()
{
	// First make cargo evasion to avoid them to lock themselve trading
	Company->GetAI()->CargosEvasion();

	// Update trade routes
	Company->GetAI()->UpdateTrading();

	// Repair and refill ships and stations
	Company->GetAI()->RepairAndRefill();

	Company->GetAI()->ProcessBudget(Company->GetAI()->AllBudgets);

	// Buy ships
	//Company->GetAI()->UpdateShipAcquisition(IdleCargoCapacity);

	Company->GetAI()->UpdateMilitaryMovement();
}

void UFlareAIBehavior::UpdateDiplomacy()
{
	ProposeTributeToPlayer = false;

	// Simulate company attitude towards others
	for (int32 CompanyIndex = 0; CompanyIndex < Game->GetGameWorld()->GetCompanies().Num(); CompanyIndex++)
	{
		UFlareCompany* OtherCompany = Game->GetGameWorld()->GetCompanies()[CompanyIndex];

		if (OtherCompany == Company)
		{
			continue;
		}

		/*if(Company == ST->AxisSupplies)
		{
			// Never declare war
			continue;
		}*/

		// Lock war with player for 10 days
		int64 DaySinceWarWithPlayer = Game->GetGameWorld()->GetDate() - Company->GetLastWarDate();
		bool LockInWar = Company->GetLastWarDate() > 0 && DaySinceWarWithPlayer < 10;
		if (OtherCompany == Game->GetPC()->GetCompany() && LockInWar)
		{
			if (Company->GetHostility(OtherCompany) != EFlareHostility::Hostile)
			{
#ifndef DEBUG_AI_NO_WAR
				Company->SetHostilityTo(OtherCompany, true);
#endif
			}
			continue;
		}

		if (Company->GetHostility(OtherCompany) == EFlareHostility::Hostile
				&& (Company->GetReputation(OtherCompany) > -100 || Company->GetConfidenceLevel(OtherCompany) < RequestPeaceConfidence))
		{
			Company->SetHostilityTo(OtherCompany, false);
		}
		else if (Company->GetHostility(OtherCompany) != EFlareHostility::Hostile
				 && Company->GetAI()->HasHealthyTradeFleet()
				 && Company->GetReputation(OtherCompany) <= -100 && Company->GetConfidenceLevel(OtherCompany) > DeclareWarConfidence)
		{
			bool CancelWar = false;
			if (OtherCompany == Game->GetPC()->GetCompany())
			{
				// Check if has quest to provide a partial imminity
				bool HasQuest = false;
				for (UFlareQuest* Quest : Game->GetQuestManager()->GetOngoingQuests())
				{
					if (Quest->GetClient() == Company)
					{
						HasQuest = true;
						break;
					}
				}

				if(HasQuest && Company->GetReputation(OtherCompany) > -180)
				{
					CancelWar = true;
				}

				if (Company->GetAI()->GetData()->Pacifism > 0)
				{
					CancelWar = true;
				}
			}

			if(!CancelWar)
			{
#ifndef DEBUG_AI_NO_WAR
				Company->SetHostilityTo(OtherCompany, true);
#endif
				if (OtherCompany == Game->GetPC()->GetCompany())
				{
					OtherCompany->SetHostilityTo(Company, true);
				}
			}
		}

		if (!LockInWar && Company->GetAI()->GetData()->Pacifism >= 100)
		{
			Company->SetHostilityTo(OtherCompany, false);
		}

		if (!LockInWar && Company->GetWarState(OtherCompany) == EFlareHostility::Hostile
				&& Company->GetConfidenceLevel(OtherCompany) < PayTributeConfidence)
		{
			if (OtherCompany == Game->GetPC()->GetCompany())
			{
				ProposeTributeToPlayer = true;
			}
			else
			{
				Company->PayTribute(OtherCompany, true);
			}
		}

	}
}

void UFlareAIBehavior::SimulatePirateBehavior()
{
	// Repair and refill ships and stations
	Company->GetAI()->RepairAndRefill();

	// First make cargo evasion to avoid them to lock themselve trading
	Company->GetAI()->CargosEvasion();

	// Update trade routes
	Company->GetAI()->UpdateTrading();

	Company->GetAI()->ProcessBudget(Company->GetAI()->AllBudgets);

	Company->GetAI()->UpdateMilitaryMovement();

}

void UFlareAIBehavior::GenerateAffilities()
{
	// Reset resource affilities
	ResourceAffilities.Empty();
	
	// Default behavior
	SetResourceAffilities(1.f);

	// If above 5, will become exclusive
	SetSectorAffilities(1.f);

	StationCapture = 1.f;
	TradingBuy = 1.f;
	TradingSell = 1.f;
	TradingBoth = 0.5f;

	ShipyardAffility = 1.0;
	ConsumerAffility = 0.5;
	MaintenanceAffility = 0.1;

	// Budget
	BudgetTechnologyWeight = 0.2;
	BudgetMilitaryWeight = 0.5;
	BudgetStationWeight = 1.0;
	BudgetTradeWeight = 1.0;

	ConfidenceTarget = -0.1;
	DeclareWarConfidence = 0.2;
	RequestPeaceConfidence = -0.5;
	PayTributeConfidence = -0.8;

	AttackThreshold = 0.99;
	RetreatThreshold = 0.5;
	DefeatAdaptation = 0.01;

	ArmySize = 5.0;
	DiplomaticReactivity = 1;

	PacifismIncrementRate = 2.0;
	PacifismDecrementRate = 1.0;

	// Pirate base
	SetSectorAffility(ST->Boneyard, 0.f);


	if(Company == ST->Pirates)
	{
		// Pirates
		// -------
		//
		// Pirate don't trade, and are only interested in getting money by force
		// All resource affilities are set to 0
		// They never build stations
		// They never research science

		// They have 2 mode : agressive or moderate

		// They need a shipyard, if they have a shipyard they will steal resources,
		// or buy one to build ship, and fill and defend their base
		// They always want graveyard

		// They need some cargo to buy resources for their shipyard and arsenal

		SetResourceAffilities(0.1f);


		SetSectorAffilities(0.f);

		SetSectorAffility(ST->Boneyard, 10.f);


		ShipyardAffility = 0.0;
		ConsumerAffility = 0.0;
		MaintenanceAffility = 0.0;


		// Only buy
		TradingSell = 0.f;
		TradingBoth = 0.f;

		// Doesn't capture station. Change in recovery
		StationCapture = 0.f;

		DeclareWarConfidence = -0.2;
		RequestPeaceConfidence = -0.8;
		PayTributeConfidence = -1.0;

		ArmySize = 50.0;
		AttackThreshold = 0.8;
		RetreatThreshold = 0.2;
		DefeatAdaptation = 0.001;

		// Budget
		BudgetTechnologyWeight = 0.0;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 0.1;
		BudgetTradeWeight = 0.1;

		PacifismIncrementRate = 3.f;
		PacifismDecrementRate = 4.f;

	}
	else if(Company == ST->GhostWorksShipyards)
	{
		// Loves Hela and doesn't like Nema

		ShipyardAffility = 5.0;

		SetSectorAffilitiesByMoon(ST->Nema,0.5f);
		SetSectorAffilitiesByMoon(ST->Hela, 6.f);

		// Budget
		BudgetTechnologyWeight = 0.2;
		BudgetMilitaryWeight = 1.0;
		BudgetStationWeight = 1.0;
		BudgetTradeWeight = 1.0;
	}
	else if(Company == ST->MiningSyndicate)
	{
		// Mining specialist.
		// Loves raw materials
		// Likes hard work
		SetResourceAffility(ST->Water, 10.f);
		SetResourceAffility(ST->Silica, 10.f);
		SetResourceAffility(ST->IronOxyde, 10.f);
		SetResourceAffility(ST->Hydrogen, 2.f);

		// Budget
		BudgetTechnologyWeight = 0.2;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;
	}
	else if(Company == ST->HelixFoundries)
	{
		// Likes hard factory, and Anka.
		// Base at Outpost
		SetResourceAffility(ST->Steel, 10.f);
		SetResourceAffility(ST->Tools, 10.f);
		SetResourceAffility(ST->Tech, 5.f);
		SetSectorAffilitiesByMoon(ST->Anka, 6.f);
		SetSectorAffility(ST->Outpost, 10.f);
		
		// Budget
		BudgetTechnologyWeight = 0.4;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;
	}
	else if(Company == ST->Sunwatch)
	{
		// Likes hard factory, and Anka.
		// Base at Outpost
		SetResourceAffility(ST->Fuel, 10.f);
		
		// Budget
		BudgetTechnologyWeight = 0.6;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;
	}
	else if(Company == ST->IonLane)
	{
		BudgetTechnologyWeight = 0.2;
		BudgetMilitaryWeight = 1.0;
		BudgetStationWeight = 0.1;
		BudgetTradeWeight = 2.0;

		ArmySize = 10.0;
		DeclareWarConfidence = 0.1;
		RequestPeaceConfidence = -0.4;
		PayTributeConfidence = -0.85;
	}
	else if(Company == ST->UnitedFarmsChemicals)
	{
		// Likes chemisty
		SetResourceAffility(ST->Food, 10.f);
		SetResourceAffility(ST->Carbon, 5.f);
		SetResourceAffility(ST->Methane, 5.f);

		// Budget
		BudgetTechnologyWeight = 0.4;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;
	}
	else if(Company == ST->NemaHeavyWorks)
	{
		// Likes Nema and heavy work
		SetResourceAffility(ST->FleetSupply, 2.f);
		SetResourceAffility(ST->Steel, 5.f);
		SetResourceAffility(ST->Tools, 5.f);
		SetResourceAffility(ST->Tech, 5.f);
		SetSectorAffilitiesByMoon(ST->Nema, 5.f);

		ShipyardAffility = 3.0;

		// Budget
		BudgetTechnologyWeight = 0.4;
		BudgetMilitaryWeight = 0.5;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;
	}
	else if(Company == ST->AxisSupplies)
	{
		// Assures fleet supply availability
		SetResourceAffility(ST->FleetSupply, 5.f);
		SetResourceAffility(ST->Food, 2.f);

		ShipyardAffility = 0.0;
		ConsumerAffility = 1.0;
		MaintenanceAffility = 10.0;

		// Budget
		BudgetTechnologyWeight = 0.2;
		BudgetMilitaryWeight = 0.25;
		BudgetStationWeight = 2.0;
		BudgetTradeWeight = 2.0;

		ArmySize = 1.0;
		DeclareWarConfidence = 1.0;
		RequestPeaceConfidence = 0.0;
		PayTributeConfidence = -0.1;

		DiplomaticReactivity = 0.1;
	}

}

void UFlareAIBehavior::SetResourceAffilities(float Value)
{
	for(int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		SetResourceAffility(Resource, Value);
	}
}

void UFlareAIBehavior::SetResourceAffility(FFlareResourceDescription* Resource, float Value)
{
	if(ResourceAffilities.Contains(Resource)){
		ResourceAffilities[Resource] = Value;
	}
	else
	{
		ResourceAffilities.Add(Resource, Value);
	}
}


void UFlareAIBehavior::SetSectorAffilities(float Value)
{
	for(int32 SectorIndex = 0; SectorIndex < Game->GetGameWorld()->GetSectors().Num(); SectorIndex++)
	{
		UFlareSimulatedSector* Sector = Game->GetGameWorld()->GetSectors()[SectorIndex];
		SetSectorAffility(Sector, Value);
	}
}

void UFlareAIBehavior::SetSectorAffility(UFlareSimulatedSector* Sector, float Value)
{
	if(SectorAffilities.Contains(Sector)){
		SectorAffilities[Sector] = Value;
	}
	else
	{
		SectorAffilities.Add(Sector, Value);
	}
}

void UFlareAIBehavior::SetSectorAffilitiesByMoon(FFlareCelestialBody *CelestialBody, float Value)
{
	for(int32 SectorIndex = 0; SectorIndex < Game->GetGameWorld()->GetSectors().Num(); SectorIndex++)
	{
		UFlareSimulatedSector* Sector = Game->GetGameWorld()->GetSectors()[SectorIndex];

		if(Sector->GetOrbitParameters()->CelestialBodyIdentifier == CelestialBody->Identifier)
		{
			SetSectorAffility(Sector, Value);
		}
	}
}


/*----------------------------------------------------
	Getters
----------------------------------------------------*/

float UFlareAIBehavior::GetBudgetWeight(EFlareBudget::Type Budget)
{
	switch (Budget) {
	case EFlareBudget::Military:
		return (Company->AtWar() ? BudgetMilitaryWeight * 10 : BudgetMilitaryWeight);
		break;
	case EFlareBudget::Trade:
		return BudgetTradeWeight;
		break;
	case EFlareBudget::Station:
		return BudgetStationWeight;
		break;
	case EFlareBudget::Technology:
		return BudgetTechnologyWeight;
		break;
	default:
		break;
	}

	return 0;
}

float UFlareAIBehavior::GetSectorAffility(UFlareSimulatedSector* Sector)
{
	if(SectorAffilities.Contains(Sector))
	{
		return SectorAffilities[Sector];
	}
	return 1.f;
}

float UFlareAIBehavior::GetResourceAffility(FFlareResourceDescription* Resource)
{
	if(ResourceAffilities.Contains(Resource))
	{
		return ResourceAffilities[Resource];
	}
	return 1.f;
}

float UFlareAIBehavior::GetAttackThreshold()
{
	return AttackThreshold + Company->GetAI()->GetData()->Caution;
}
