
#include "FlareSectorHelper.h"
#include "../Flare.h"

#include "FlareCompany.h"
#include "../Game/FlareGame.h"
#include "../Game/FlareWorld.h"
#include "../Economy/FlareCargoBay.h"
#include "../Player/FlarePlayerController.h"
#include "../Game/FlareScenarioTools.h"

UFlareSimulatedSpacecraft*  SectorHelper::FindTradeStation(FlareTradeRequest Request)
{
	if(!Request.Client || !Request.Client->GetCurrentSector())
	{
		FLOG("Invalid find trade query");
		return NULL;
	}

	UFlareSimulatedSector* Sector = Request.Client->GetCurrentSector();
	TArray<UFlareSimulatedSpacecraft*>& SectorStations = Sector->GetSectorStations();

	float UnloadQuantityScoreMultiplier = 0;
	float LoadQuantityScoreMultiplier = 0;
	float SellQuantityScoreMultiplier = 0;
	float BuyQuantityScoreMultiplier = 0;
	float FullRatioBonus = 0;
	float EmptyRatioBonus = 0;
	bool  NeedInput = false;
	bool  NeedOutput = false;

	switch(Request.Operation)
	{
		case EFlareTradeRouteOperation::Buy:
			BuyQuantityScoreMultiplier = 10.f;
			FullRatioBonus = 0.1;
			NeedOutput = true;
		break;
		case EFlareTradeRouteOperation::Sell:
			SellQuantityScoreMultiplier = 10.f;
			EmptyRatioBonus = 0.1;
			NeedInput = true;
		break;
		case EFlareTradeRouteOperation::Load:
			LoadQuantityScoreMultiplier = 10.f;
			FullRatioBonus = 0.1;
			NeedOutput = true;
		break;
		case EFlareTradeRouteOperation::Unload:
			UnloadQuantityScoreMultiplier = 10.f;
			EmptyRatioBonus = 0.1;
			NeedInput = true;
		break;
		case EFlareTradeRouteOperation::LoadOrBuy:
			LoadQuantityScoreMultiplier = 10.f;
			BuyQuantityScoreMultiplier = 1.f;
			FullRatioBonus = 0.1;
			NeedOutput = true;
		break;
		case EFlareTradeRouteOperation::UnloadOrSell:
			UnloadQuantityScoreMultiplier = 10.f;
			SellQuantityScoreMultiplier = 1.f;
			EmptyRatioBonus = 0.1;
			NeedInput = true;
		break;
	}

	float BestScore = 0;
	UFlareSimulatedSpacecraft* BestStation = NULL;
	uint32 AvailableQuantity = Request.Client->GetCargoBay()->GetResourceQuantity(Request.Resource, Request.Client->GetCompany());
	uint32 FreeSpace = Request.Client->GetCargoBay()->GetFreeSpaceForResource(Request.Resource, Request.Client->GetCompany());

	for (int32 StationIndex = 0; StationIndex < SectorStations.Num(); StationIndex++)
	{
		UFlareSimulatedSpacecraft* Station = SectorStations[StationIndex];

		FText Unused;
		if(!Request.Client->CanTradeWith(Station, Unused))
		{
			continue;
		}
		EFlareResourcePriceContext::Type StationResourceUsage = Station->GetResourceUseType(Request.Resource);

		if(NeedOutput && (StationResourceUsage != EFlareResourcePriceContext::FactoryOutput &&
						  StationResourceUsage != EFlareResourcePriceContext::MaintenanceConsumption))
		{
			continue;
		}

		if(NeedInput && (StationResourceUsage != EFlareResourcePriceContext::FactoryInput &&
						 StationResourceUsage != EFlareResourcePriceContext::ConsumerConsumption &&
						 StationResourceUsage != EFlareResourcePriceContext::MaintenanceConsumption))
		{
			continue;
		}

		uint32 StationFreeSpace = Station->GetCargoBay()->GetFreeSpaceForResource(Request.Resource, Request.Client->GetCompany());
		uint32 StationResourceQuantity = Station->GetCargoBay()->GetResourceQuantity(Request.Resource, Request.Client->GetCompany());

		if (StationFreeSpace == 0 && StationResourceQuantity == 0)
		{
			continue;
		}

		float Score = 0;
		float FullRatio =  (float) StationResourceQuantity / (float) (StationResourceQuantity + StationFreeSpace);
		float EmptyRatio = 1 - FullRatio;
		uint32 UnloadMaxQuantity  = 0;
		uint32 LoadMaxQuantity  = 0;


		if(!Station->IsUnderConstruction())
		{
			// Check cargo limit
			if(NeedOutput && Request.CargoLimit != -1 && FullRatio < Request.CargoLimit / Station->GetLevel())
			{
				continue;
			}

			if(NeedInput && Request.CargoLimit != -1 && FullRatio > (1.f - (1.f - Request.CargoLimit) / Station->GetLevel()))
			{
				continue;
			}
		}

		if(Station->GetCargoBay()->WantBuy(Request.Resource, Request.Client->GetCompany()))
		{
			UnloadMaxQuantity = StationFreeSpace;
			UnloadMaxQuantity  = FMath::Min(UnloadMaxQuantity , AvailableQuantity);
		}

		if(Station->GetCargoBay()->WantSell(Request.Resource, Request.Client->GetCompany()))
		{
			LoadMaxQuantity = StationResourceQuantity;
			LoadMaxQuantity = FMath::Min(LoadMaxQuantity , FreeSpace);
		}

		if(Station->GetCompany() == Request.Client->GetCompany())
		{
			Score += UnloadMaxQuantity * UnloadQuantityScoreMultiplier;
			Score += LoadMaxQuantity * LoadQuantityScoreMultiplier;
		}
		else
		{
			EFlareResourcePriceContext::Type ResourceUsage = Station->GetResourceUseType(Request.Resource);

			uint32 MaxBuyableQuantity = Request.Client->GetCompany()->GetMoney() / Sector->GetResourcePrice(Request.Resource, ResourceUsage);
			LoadMaxQuantity = FMath::Min(LoadMaxQuantity , MaxBuyableQuantity);

			uint32 MaxSellableQuantity = Station->GetCompany()->GetMoney() / Sector->GetResourcePrice(Request.Resource, ResourceUsage);
			UnloadMaxQuantity = FMath::Min(UnloadMaxQuantity , MaxSellableQuantity);

			Score += UnloadMaxQuantity * SellQuantityScoreMultiplier;
			Score += LoadMaxQuantity * BuyQuantityScoreMultiplier;
		}

		Score *= 1 + (FullRatio * FullRatioBonus) + (EmptyRatio * EmptyRatioBonus);

		if(Station->IsUnderConstruction())
		{
			Score *= 10000;
			FLOGV("Station %s is under construction. Score %f, BestScore %f",
				  *Station->GetImmatriculation().ToString(),
				  Score,
				  BestScore)
		}

		if(Score > 0 && Score > BestScore)
		{
			BestScore = Score;
			BestStation = Station;
		}
	}

	return BestStation;
}

int32 SectorHelper::Trade(UFlareSimulatedSpacecraft* SourceSpacecraft, UFlareSimulatedSpacecraft* DestinationSpacecraft, FFlareResourceDescription* Resource, int32 MaxQuantity)
{
	FText Unused;
	if(!SourceSpacecraft->CanTradeWith(DestinationSpacecraft, Unused))
	{
		FLOG("Both spacecraft cannot trade");
		return 0;
	}

	int32 ResourcePrice = SourceSpacecraft->GetCurrentSector()->GetTransfertResourcePrice(SourceSpacecraft, DestinationSpacecraft, Resource);
	int32 QuantityToTake = MaxQuantity;

	if (SourceSpacecraft->GetCompany() != DestinationSpacecraft->GetCompany())
	{
		// Limit transaction bay available money
		int32 MaxAffordableQuantity = FMath::Max(0, int32(DestinationSpacecraft->GetCompany()->GetMoney() / ResourcePrice));
		QuantityToTake = FMath::Min(QuantityToTake, MaxAffordableQuantity);
	}
	int32 ResourceCapacity = DestinationSpacecraft->GetCargoBay()->GetFreeSpaceForResource(Resource, SourceSpacecraft->GetCompany());

	QuantityToTake = FMath::Min(QuantityToTake, ResourceCapacity);
	
	int32 TakenResources = SourceSpacecraft->GetCargoBay()->TakeResources(Resource, QuantityToTake, DestinationSpacecraft->GetCompany());
	int32 GivenResources = DestinationSpacecraft->GetCargoBay()->GiveResources(Resource, TakenResources, SourceSpacecraft->GetCompany());

	// Pay
	if (GivenResources > 0 && SourceSpacecraft->GetCompany() != DestinationSpacecraft->GetCompany())
	{
		int64 Price = ResourcePrice * GivenResources;
		DestinationSpacecraft->GetCompany()->TakeMoney(Price);
		SourceSpacecraft->GetCompany()->GiveMoney(Price);

		SourceSpacecraft->GetCompany()->GiveReputation(DestinationSpacecraft->GetCompany(), 0.0001f, false);
		DestinationSpacecraft->GetCompany()->GiveReputation(SourceSpacecraft->GetCompany(), 0.0001f, false);
	}
	
	// Set the trading state if not player fleet
	if (GivenResources > 0)
	{
		AFlarePlayerController* PC = SourceSpacecraft->GetGame()->GetPC();
		UFlareFleet* PlayerFleet = PC->GetPlayerFleet();
		FCHECK(PC);

		if (SourceSpacecraft->GetCurrentFleet() != PlayerFleet && !SourceSpacecraft->IsStation())
		{
			SourceSpacecraft->SetTrading(true);
		}

		if (DestinationSpacecraft->GetCurrentFleet() != PlayerFleet && !DestinationSpacecraft->IsStation())
		{
			DestinationSpacecraft->SetTrading(true);
		}
	}

	SourceSpacecraft->GetGame()->GetQuestManager()->OnTradeDone(SourceSpacecraft, DestinationSpacecraft, Resource, GivenResources);

	return GivenResources;

}

void SectorHelper::GetAvailableFleetSupplyCount(UFlareSimulatedSector* Sector, UFlareCompany* Company, int32& OwnedFS, int32& AvailableFS, int32& AffordableFS)
{
	OwnedFS = 0;
	int32 NotOwnedFS = 0;

	FFlareResourceDescription* FleetSupply = Sector->GetGame()->GetScenarioTools()->FleetSupply;

	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		if (Company->GetWarState(Spacecraft->GetCompany()) == EFlareHostility::Hostile)
		{
			// At war, no trade possible
			continue;
		}

		int32 AvailableQuantity = Spacecraft->GetCargoBay()->GetResourceQuantity(FleetSupply, Company);

		if (Company == Spacecraft->GetCompany())
		{
			OwnedFS += AvailableQuantity;
		}
		else
		{
			NotOwnedFS += AvailableQuantity;
		}
	}

	AvailableFS = OwnedFS + NotOwnedFS;
	int32 ResourcePrice = Sector->GetResourcePrice(FleetSupply, EFlareResourcePriceContext::MaintenanceConsumption);

	int32 MaxAffordableQuantity = FMath::Max(0, int32(Company->GetMoney() / ResourcePrice));

	AffordableFS = OwnedFS + FMath::Min(MaxAffordableQuantity, NotOwnedFS);
}

float SectorHelper::GetComponentMaxRepairRatio(FFlareSpacecraftComponentDescription* ComponentDescription)
{
	switch(ComponentDescription->Type)
	{
	case EFlarePartType::RCS:
		return MAX_RCS_REPAIR_RATIO_BY_DAY;
		break;
	case EFlarePartType::OrbitalEngine:
		return MAX_ENGINE_REPAIR_RATIO_BY_DAY;
		break;
	case EFlarePartType::Weapon:
		return MAX_WEAPON_REPAIR_RATIO_BY_DAY;
		break;
	default:
		return MAX_REPAIR_RATIO_BY_DAY;
	}
}

bool SectorHelper::HasShipRefilling(UFlareSimulatedSector* TargetSector, UFlareCompany* Company)
{
	for (UFlareSimulatedSpacecraft* Spacecraft : TargetSector->GetSectorSpacecrafts())
	{
		if (Company != Spacecraft->GetCompany()) {
			continue;
		}

		if (Spacecraft->GetRefillStock() > 0 && Spacecraft->NeedRefill())
		{
			return true;
		}

	}
	return false;
}

bool SectorHelper::HasShipRepairing(UFlareSimulatedSector* TargetSector, UFlareCompany* Company)
{
	for (UFlareSimulatedSpacecraft* Spacecraft : TargetSector->GetSectorSpacecrafts())
	{
		if (Company != Spacecraft->GetCompany()) {
			continue;
		}

		if (Spacecraft->GetRepairStock() > 0 && Spacecraft->GetDamageSystem()->GetGlobalHealth() < 1.f)
		{
			return true;
		}
	}
	return false;
}


void SectorHelper::GetRepairFleetSupplyNeeds(UFlareSimulatedSector* Sector, UFlareCompany* Company, int32& CurrentNeededFleetSupply, int32& TotalNeededFleetSupply, int64& MaxDuration)
{
	float PreciseCurrentNeededFleetSupply = 0;
	float PreciseTotalNeededFleetSupply = 0;
	MaxDuration = 0;
	UFlareSpacecraftComponentsCatalog* Catalog = Company->GetGame()->GetShipPartsCatalog();



	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		if (Company != Spacecraft->GetCompany()) {
			continue;
		}

		if (!Spacecraft->GetDamageSystem()->IsAlive()) {
			continue;
		}

		float SpacecraftPreciseCurrentNeededFleetSupply = 0;
		float SpacecraftPreciseTotalNeededFleetSupply = 0;

		// List components
		for (int32 ComponentIndex = 0; ComponentIndex < Spacecraft->GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &Spacecraft->GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			float DamageRatio = Spacecraft->GetDamageSystem()->GetDamageRatio(ComponentDescription, ComponentData);

			float TechnologyBonus = Company->IsTechnologyUnlocked("quick-repair") ? 1.5f: 1.f;

			float ComponentMaxRepairRatio = GetComponentMaxRepairRatio(ComponentDescription) * TechnologyBonus;

			float CurrentRepairRatio = FMath::Min(ComponentMaxRepairRatio, (1.f - DamageRatio));
			float TotalRepairRatio = 1.f - DamageRatio;

			int64 Duration =  FMath::CeilToInt(TotalRepairRatio / ComponentMaxRepairRatio);
			if(Duration > MaxDuration)
			{
				MaxDuration = Duration;
			}

			SpacecraftPreciseCurrentNeededFleetSupply += CurrentRepairRatio * UFlareSimulatedSpacecraftDamageSystem::GetRepairCost(ComponentDescription);
			SpacecraftPreciseTotalNeededFleetSupply += TotalRepairRatio * UFlareSimulatedSpacecraftDamageSystem::GetRepairCost(ComponentDescription);
		}

		PreciseCurrentNeededFleetSupply += FMath::Max(0.f, SpacecraftPreciseCurrentNeededFleetSupply - Spacecraft->GetRepairStock());
		PreciseTotalNeededFleetSupply += FMath::Max(0.f, SpacecraftPreciseTotalNeededFleetSupply - Spacecraft->GetRepairStock());
	}

	// Round to ceil

	if(PreciseCurrentNeededFleetSupply < 0.001)
	{
		PreciseCurrentNeededFleetSupply = 0;
	}

	if(PreciseTotalNeededFleetSupply < 0.001)
	{
		PreciseTotalNeededFleetSupply = 0;
	}
	CurrentNeededFleetSupply = FMath::CeilToInt(PreciseCurrentNeededFleetSupply);
	TotalNeededFleetSupply = FMath::CeilToInt(PreciseTotalNeededFleetSupply);
}

void SectorHelper::GetRefillFleetSupplyNeeds(UFlareSimulatedSector* Sector, UFlareCompany* Company, int32& CurrentNeededFleetSupply, int32& TotalNeededFleetSupply, int64& MaxDuration)
{
	float PreciseCurrentNeededFleetSupply = 0;
	float PreciseTotalNeededFleetSupply = 0;
	MaxDuration = 0;
	UFlareSpacecraftComponentsCatalog* Catalog = Company->GetGame()->GetShipPartsCatalog();

	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		if (Company != Spacecraft->GetCompany()) {
			continue;
		}

		if (!Spacecraft->GetDamageSystem()->IsAlive()) {
			continue;
		}

		float SpacecraftPreciseCurrentNeededFleetSupply = 0;
		float SpacecraftPreciseTotalNeededFleetSupply = 0;

		// List components
		for (int32 ComponentIndex = 0; ComponentIndex < Spacecraft->GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &Spacecraft->GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			if(ComponentDescription->Type == EFlarePartType::Weapon)
			{

				int32 MaxAmmo = ComponentDescription->WeaponCharacteristics.AmmoCapacity;
				int32 CurrentAmmo = MaxAmmo - ComponentData->Weapon.FiredAmmo;

				float FillRatio = (float) CurrentAmmo / (float) MaxAmmo;


				float CurrentRefillRatio = FMath::Min(MAX_REFILL_RATIO_BY_DAY, (1.f - FillRatio));
				float TotalRefillRatio = 1.f - FillRatio;

				int64 Duration =  FMath::CeilToInt(TotalRefillRatio / MAX_REFILL_RATIO_BY_DAY);
				if(Duration > MaxDuration)
				{
					MaxDuration = Duration;
				}

				SpacecraftPreciseCurrentNeededFleetSupply += CurrentRefillRatio * UFlareSimulatedSpacecraftDamageSystem::GetRefillCost(ComponentDescription);
				SpacecraftPreciseTotalNeededFleetSupply += TotalRefillRatio * UFlareSimulatedSpacecraftDamageSystem::GetRefillCost(ComponentDescription);
			}
		}

		PreciseCurrentNeededFleetSupply += FMath::Max(0.f, SpacecraftPreciseCurrentNeededFleetSupply - Spacecraft->GetRefillStock());
		PreciseTotalNeededFleetSupply += FMath::Max(0.f, SpacecraftPreciseTotalNeededFleetSupply - Spacecraft->GetRefillStock());
	}

	// Round to ceil
	CurrentNeededFleetSupply = FMath::CeilToInt(PreciseCurrentNeededFleetSupply);
	TotalNeededFleetSupply = FMath::CeilToInt(PreciseTotalNeededFleetSupply);
}


void SectorHelper::RepairFleets(UFlareSimulatedSector* Sector, UFlareCompany* Company)
{
	int32 CurrentNeededFleetSupply;
	int32 TotalNeededFleetSupply;
	int32 OwnedFS;
	int32 AvailableFS;
	int32 AffordableFS;
	int64 MaxDuration;


	GetRepairFleetSupplyNeeds(Sector, Company, CurrentNeededFleetSupply, TotalNeededFleetSupply, MaxDuration);
	GetAvailableFleetSupplyCount(Sector, Company, OwnedFS, AvailableFS, AffordableFS);


	// Note not available fleet supply as consumed
	Sector->OnFleetSupplyConsumed(FMath::Max(0, TotalNeededFleetSupply - AvailableFS));



	if(Sector->IsInDangerousBattle(Company) || AffordableFS == 0 || TotalNeededFleetSupply == 0)
	{
		// No repair possible
		return;
	}

	float RepairRatio = FMath::Min(1.f,(float) AffordableFS /  (float) TotalNeededFleetSupply);
	float RemainingFS = (float) AffordableFS;
	UFlareSpacecraftComponentsCatalog* Catalog = Company->GetGame()->GetShipPartsCatalog();


	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		if (Company != Spacecraft->GetCompany() || !Spacecraft->GetDamageSystem()->IsAlive()) {
			continue;
		}

		float SpacecraftPreciseTotalNeededFleetSupply = 0;


		// List components
		for (int32 ComponentIndex = 0; ComponentIndex < Spacecraft->GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &Spacecraft->GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			float DamageRatio = Spacecraft->GetDamageSystem()->GetDamageRatio(ComponentDescription, ComponentData);
			float TotalRepairRatio = 1.f - DamageRatio;

			SpacecraftPreciseTotalNeededFleetSupply += TotalRepairRatio * UFlareSimulatedSpacecraftDamageSystem::GetRepairCost(ComponentDescription);


		}

		float SpacecraftNeededWithoutStock = SpacecraftPreciseTotalNeededFleetSupply - Spacecraft->GetRepairStock();
		float SpacecraftNeededWithoutStockScaled = FMath::Max(0.f, SpacecraftNeededWithoutStock * RepairRatio);
		float ConsumedFS = FMath::Min(RemainingFS, SpacecraftNeededWithoutStockScaled);
		Spacecraft->OrderRepairStock(ConsumedFS);
		RemainingFS -= ConsumedFS;

		if(RemainingFS <= 0)
		{
			break;
		}
	}

	int32 ConsumedFS = FMath::CeilToInt((float) AffordableFS - RemainingFS);
	ConsumeFleetSupply(Sector, Company, ConsumedFS);
}

void SectorHelper::RefillFleets(UFlareSimulatedSector* Sector, UFlareCompany* Company)
{
	int32 CurrentNeededFleetSupply;
	int32 TotalNeededFleetSupply;
	int32 OwnedFS;
	int32 AvailableFS;
	int32 AffordableFS;
	int64 MaxDuration;

	GetRefillFleetSupplyNeeds(Sector, Company, CurrentNeededFleetSupply, TotalNeededFleetSupply, MaxDuration);
	GetAvailableFleetSupplyCount(Sector, Company, OwnedFS, AvailableFS, AffordableFS);


	// Note not available fleet supply as consumed
	Sector->OnFleetSupplyConsumed(FMath::Max(0, TotalNeededFleetSupply - AvailableFS));

	if(Sector->IsInDangerousBattle(Company) || AffordableFS == 0 || TotalNeededFleetSupply == 0)
	{
		// No refill possible
		return;
	}

	float MaxRefillRatio = FMath::Min(1.f,(float) AffordableFS /  (float) TotalNeededFleetSupply);
	float RemainingFS = (float) AffordableFS;
	UFlareSpacecraftComponentsCatalog* Catalog = Company->GetGame()->GetShipPartsCatalog();

	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		if (Company != Spacecraft->GetCompany() || !Spacecraft->GetDamageSystem()->IsAlive()) {
			continue;
		}

		float SpacecraftPreciseTotalNeededFleetSupply = 0;

		// List components
		for (int32 ComponentIndex = 0; ComponentIndex < Spacecraft->GetData().Components.Num(); ComponentIndex++)
		{
			FFlareSpacecraftComponentSave* ComponentData = &Spacecraft->GetData().Components[ComponentIndex];
			FFlareSpacecraftComponentDescription* ComponentDescription = Catalog->Get(ComponentData->ComponentIdentifier);

			if(ComponentDescription->Type == EFlarePartType::Weapon)
			{
				int32 MaxAmmo = ComponentDescription->WeaponCharacteristics.AmmoCapacity;
				int32 CurrentAmmo = MaxAmmo - ComponentData->Weapon.FiredAmmo;

				float FillRatio = (float) CurrentAmmo / (float) MaxAmmo;

				float TotalRefillRatio = 1.f - FillRatio;

				SpacecraftPreciseTotalNeededFleetSupply += TotalRefillRatio * UFlareSimulatedSpacecraftDamageSystem::GetRefillCost(ComponentDescription);
			}
		}

		float SpacecraftNeededWithoutStock = SpacecraftPreciseTotalNeededFleetSupply - Spacecraft->GetRefillStock();
		float SpacecraftNeededWithoutStockScaled = FMath::Max(0.f, SpacecraftNeededWithoutStock * MaxRefillRatio);

		float ConsumedFS = FMath::Min(RemainingFS, SpacecraftNeededWithoutStockScaled);

		Spacecraft->OrderRefillStock(ConsumedFS);
		RemainingFS -= ConsumedFS;

		if(RemainingFS <= 0)
		{
			break;
		}
	}

	int32 ConsumedFS = FMath::CeilToInt((float) AffordableFS - RemainingFS);

	ConsumeFleetSupply(Sector, Company, ConsumedFS);
}

void SectorHelper::ConsumeFleetSupply(UFlareSimulatedSector* Sector, UFlareCompany* Company, int32 ConsumedFS)
{
	// First check for owned FS
	Sector->OnFleetSupplyConsumed(ConsumedFS);

	FFlareResourceDescription* FleetSupply = Sector->GetGame()->GetScenarioTools()->FleetSupply;

	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		if (Company != Spacecraft->GetCompany())
		{
			continue;
		}

		int TakenQuantity = Spacecraft->GetCargoBay()->TakeResources(FleetSupply, ConsumedFS, Company);
		ConsumedFS -= TakenQuantity;

		if(ConsumedFS == 0)
		{
			return;
		}
	}

	for (int32 SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		if (Company == Spacecraft->GetCompany())
		{
			continue;
		}

		if (Company->GetWarState(Spacecraft->GetCompany()) == EFlareHostility::Hostile)
		{
			// At war, no trade possible
			continue;
		}

		int TakenQuantity = Spacecraft->GetCargoBay()->TakeResources(FleetSupply, ConsumedFS, Company);

		if(TakenQuantity > 0)
		{
			int32 ResourcePrice = Sector->GetResourcePrice(FleetSupply, EFlareResourcePriceContext::MaintenanceConsumption);

			int64 Cost = TakenQuantity * ResourcePrice;
			Company->TakeMoney(Cost, true);
			Spacecraft->GetCompany()->GiveMoney(Cost);

			ConsumedFS -= TakenQuantity;

			if(ConsumedFS == 0)
			{
				return;
			}
		}
	}
}

int32 SectorHelper::GetArmyCombatPoints(UFlareSimulatedSector* Sector, bool ReduceByDamage)
{
	int32 AllCombatPoints = 0;

	for (UFlareSimulatedSpacecraft* Spacecraft : Sector->GetSectorSpacecrafts())
	{
		AllCombatPoints += Spacecraft->GetCombatPoints(ReduceByDamage);
	}
	return AllCombatPoints;
}

int32 SectorHelper::GetHostileArmyCombatPoints(UFlareSimulatedSector* Sector, UFlareCompany* Company, bool ReduceByDamage)
{
	int32 HostileCombatPoints = 0;

	for(UFlareSimulatedSpacecraft* Spacecraft: Sector->GetSectorSpacecrafts())
	{
		if(Spacecraft->GetCompany()->GetWarState(Company) != EFlareHostility::Hostile)
		{
			continue;
		}
		HostileCombatPoints += Spacecraft->GetCombatPoints(ReduceByDamage);
	}
	return HostileCombatPoints;
}

int32 SectorHelper::GetCompanyArmyCombatPoints(UFlareSimulatedSector* Sector, UFlareCompany* Company, bool ReduceByDamage)
{
	int32 CompanyCombatPoints = 0;

	for(UFlareSimulatedSpacecraft* Spacecraft: Sector->GetSectorSpacecrafts())
	{
		if(Spacecraft->GetCompany() != Company)
		{
			continue;
		}
		CompanyCombatPoints += Spacecraft->GetCombatPoints(ReduceByDamage);
	}
	return CompanyCombatPoints;
}


TMap<FFlareResourceDescription*, WorldHelper::FlareResourceStats> SectorHelper::ComputeSectorResourceStats(UFlareSimulatedSector* Sector)
{
	TMap<FFlareResourceDescription*, WorldHelper::FlareResourceStats> WorldStats;

	// Init
	for(int32 ResourceIndex = 0; ResourceIndex < Sector->GetGame()->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Sector->GetGame()->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		WorldHelper::FlareResourceStats ResourceStats;
		ResourceStats.Production = 0;
		ResourceStats.Consumption = 0;
		ResourceStats.Balance = 0;
		ResourceStats.Stock = 0;
		ResourceStats.Capacity = 0;

		WorldStats.Add(Resource, ResourceStats);
	}

	for (int SpacecraftIndex = 0; SpacecraftIndex < Sector->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareSimulatedSpacecraft* Spacecraft = Sector->GetSectorSpacecrafts()[SpacecraftIndex];

		// Stock
		TArray<FFlareCargo>& CargoBaySlots = Spacecraft->GetCargoBay()->GetSlots();
		for (int CargoIndex = 0; CargoIndex < CargoBaySlots.Num(); CargoIndex++)
		{
			FFlareCargo& Cargo = CargoBaySlots[CargoIndex];

			if (!Cargo.Resource)
			{
				continue;
			}

			WorldHelper::FlareResourceStats *ResourceStats = &WorldStats[Cargo.Resource];

			switch (Spacecraft->GetResourceUseType(Cargo.Resource))
			{
				case EFlareResourcePriceContext::FactoryInput:
				case EFlareResourcePriceContext::ConsumerConsumption:
					ResourceStats->Capacity += Spacecraft->GetCargoBay()->GetSlotCapacity() - Cargo.Quantity;
				break;
				case EFlareResourcePriceContext::FactoryOutput:
					ResourceStats->Stock += Cargo.Quantity;
				break;
				case EFlareResourcePriceContext::MaintenanceConsumption:
					ResourceStats->Capacity += Spacecraft->GetCargoBay()->GetSlotCapacity() - Cargo.Quantity;
					ResourceStats->Stock += Cargo.Quantity;
				break;
			}
		}

		for (int32 FactoryIndex = 0; FactoryIndex < Spacecraft->GetFactories().Num(); FactoryIndex++)
		{
			UFlareFactory* Factory = Spacecraft->GetFactories()[FactoryIndex];
			if ((!Factory->IsActive() || !Factory->IsNeedProduction()))
			{
				// No resources needed
				break;
			}

			// Input flow
			for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetInputResourcesCount(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = Factory->GetInputResource(ResourceIndex);
				WorldHelper::FlareResourceStats *ResourceStats = &WorldStats[Resource];

				int64 ProductionDuration = Factory->GetProductionDuration();

				float Flow = 0;

				if (ProductionDuration == 0)
				{
					Flow = 1;
				}
				else
				{
					Flow = (float) Factory->GetInputResourceQuantity(ResourceIndex) / float(ProductionDuration);
				}

				ResourceStats->Consumption += Flow;
			}

			// Ouput flow
			for (int32 ResourceIndex = 0; ResourceIndex < Factory->GetOutputResourcesCount(); ResourceIndex++)
			{
				FFlareResourceDescription* Resource = Factory->GetOutputResource(ResourceIndex);
				WorldHelper::FlareResourceStats *ResourceStats = &WorldStats[Resource];

				int64 ProductionDuration = Factory->GetProductionDuration();
				if (ProductionDuration == 0)
				{
					ProductionDuration = 10;
				}

				float Flow = (float) Factory->GetOutputResourceQuantity(ResourceIndex) / float(ProductionDuration);
				ResourceStats->Production+= Flow;
			}
		}
	}

	// FS
	FFlareResourceDescription* FleetSupply = Sector->GetGame()->GetScenarioTools()->FleetSupply;
	WorldHelper::FlareResourceStats *FSResourceStats = &WorldStats[FleetSupply];
	FFlareFloatBuffer* Stats = &Sector->GetData()->FleetSupplyConsumptionStats;
	float MeanConsumption = Stats->GetMean(0, Stats->MaxSize-1);
	FSResourceStats->Consumption += MeanConsumption;
	FSResourceStats->Capacity += Stats->GetValue(0);


	// Customer flow
	for (int32 ResourceIndex = 0; ResourceIndex < Sector->GetGame()->GetResourceCatalog()->ConsumerResources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Sector->GetGame()->GetResourceCatalog()->ConsumerResources[ResourceIndex]->Data;
		WorldHelper::FlareResourceStats *ResourceStats = &WorldStats[Resource];

		ResourceStats->Consumption += Sector->GetPeople()->GetRessourceConsumption(Resource, false);
	}

	// Balance
	for(int32 ResourceIndex = 0; ResourceIndex < Sector->GetGame()->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Sector->GetGame()->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		WorldHelper::FlareResourceStats *ResourceStats = &WorldStats[Resource];

		ResourceStats->Balance = ResourceStats->Production - ResourceStats->Consumption;

		/*FLOGV("World stats for %s: Production=%f Consumption=%f Balance=%f Stock=%d",
			  *Resource->Name.ToString(),
			  ResourceStats->Production,
			  ResourceStats->Consumption,
			  ResourceStats->Balance,
			  ResourceStats->Stock);*/
	}

	return WorldStats;

}
