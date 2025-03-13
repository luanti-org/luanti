// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include "gamedef.h"
#include "inventory.h"

/*
	Crafting methods.

	The crafting method depends on the inventory list
	that the crafting input comes from.
*/
enum CraftMethod
{
	// Crafting grid
	CRAFT_METHOD_NORMAL,
	// Cooking something in a furnace
	CRAFT_METHOD_COOKING,
	// Using something as fuel for a furnace
	CRAFT_METHOD_FUEL,
};

/*
	The type a hash can be. The earlier a type is mentioned in this enum,
	the earlier it is tried at crafting, and the less likely is a collision.
	Changing order causes changes in behavior, so know what you do.
 */
enum CraftHashType
{
	// Hashes the normalized names of the recipe's elements.
	// Only recipes without group usage can be found here,
	// because groups can't be guessed efficiently.
	CRAFT_HASH_TYPE_ITEM_NAMES,

	// Counts the non-empty slots.
	CRAFT_HASH_TYPE_COUNT,

	// This layer both spares an extra variable, and helps to retain (albeit rarely used) functionality. Maps to 0.
	// Before hashes are "initialized", all hashes reside here, after initialisation, none are.
	CRAFT_HASH_TYPE_UNHASHED

};
const int craft_hash_type_max = (int) CRAFT_HASH_TYPE_UNHASHED;

/*
	Input: The contents of the crafting slots, arranged in matrix form
*/
struct CraftInput
{
	CraftMethod method = CRAFT_METHOD_NORMAL;
	unsigned int width = 0;
	std::vector<ItemStack> items;

	CraftInput() = default;

	CraftInput(CraftMethod method_, unsigned int width_,
			const std::vector<ItemStack> &items_):
		method(method_), width(width_), items(items_)
	{}

	// Returns true if all items are empty.
	bool empty() const;

	std::string dump() const;
};

/*
	Output: Result of crafting operation
*/
struct CraftOutput
{
	// Used for normal crafting and cooking, itemstring
	std::string item = "";
	// Used for cooking (cook time) and fuel (burn time), seconds
	float time = 0.0f;

	CraftOutput() = default;

	CraftOutput(const std::string &item_, float time_):
		item(item_), time(time_)
	{}
	std::string dump() const;
};

/*
	A list of replacements. A replacement indicates that a specific
	input item should not be deleted (when crafting) but replaced with
	a different item. Each replacements is a pair (itemstring to remove,
	itemstring to replace with)

	Example: If ("bucket:bucket_water", "bucket:bucket_empty") is a
	replacement pair, the crafting input slot that contained a water
	bucket will contain an empty bucket after crafting.
*/
struct CraftReplacements
{
	// List of replacements
	std::vector<std::pair<std::string, std::string> > pairs;

	CraftReplacements() = default;
	CraftReplacements(const std::vector<std::pair<std::string, std::string> > &pairs_):
		pairs(pairs_)
	{}
	std::string dump() const;
};

/*
	Crafting definition base class
*/
class CraftDefinition
{
public:
	/*
		Craft recipe priorities, from low to high

		Recipes are searched from latest to first.
		If a recipe with higher priority than a previous found one is
		encountered, it is selected instead.
	*/
	enum RecipePriority
	{
		PRIORITY_NO_RECIPE,
		PRIORITY_TOOLREPAIR,
		PRIORITY_SHAPELESS_AND_GROUPS,
		PRIORITY_SHAPELESS,
		PRIORITY_SHAPED_AND_GROUPS,
		PRIORITY_SHAPED,
	};

	CraftDefinition() = default;
	virtual ~CraftDefinition() = default;

	// Returns type of crafting definition
	virtual std::string getName() const=0;

	// Checks whether the recipe is applicable
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const=0;
	RecipePriority getPriority() const
	{
		return priority;
	}
	// Returns the output structure, meaning depends on crafting method
	// The implementation can assume that check(input) returns true
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const=0;
	// the inverse of the above
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const=0;
	// Decreases count of every input item
	virtual void decrementInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements, IGameDef *gamedef) const=0;

	CraftHashType getHashType() const
	{
		return hash_type;
	}
	virtual u64 getHash(CraftHashType type) const = 0;

	// to be called after all mods are loaded, so that we catch all aliases
	virtual void initHash(IGameDef *gamedef) = 0;

	virtual std::string dump() const=0;

protected:
	CraftHashType hash_type;
	RecipePriority priority;
};

/*
	A plain-jane (shaped) crafting definition

	Supported crafting method: CRAFT_METHOD_NORMAL.
	Requires the input items to be arranged exactly like in the recipe.
*/
class CraftDefinitionShaped: public CraftDefinition
{
public:
	CraftDefinitionShaped() = delete;
	CraftDefinitionShaped(
		const std::string &output_,
		unsigned int width_,
		const std::vector<std::string> &recipe_,
		const CraftReplacements &replacements_);

	virtual ~CraftDefinitionShaped() = default;

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements, IGameDef *gamedef) const;

	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

private:
	// Output itemstring
	std::string output = "";
	// Width of recipe
	unsigned int width = 1;
	// Recipe matrix (itemstrings)
	std::vector<std::string> recipe;
	// Recipe matrix (item names)
	std::vector<std::string> recipe_names;
	// bool indicating if initHash has been called already
	bool hash_inited = false;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	A shapeless crafting definition
	Supported crafting method: CRAFT_METHOD_NORMAL.
	Input items can arranged in any way.
*/
class CraftDefinitionShapeless: public CraftDefinition
{
public:
	CraftDefinitionShapeless() = delete;
	CraftDefinitionShapeless(
		const std::string &output_,
		const std::vector<std::string> &recipe_,
		const CraftReplacements &replacements_);

	virtual ~CraftDefinitionShapeless() = default;

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements, IGameDef *gamedef) const;

	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

private:
	// Output itemstring
	std::string output;
	// Recipe list (itemstrings)
	std::vector<std::string> recipe;
	// Recipe list (item names), sorted
	std::vector<std::string> recipe_names;
	// bool indicating if initHash has been called already
	bool hash_inited = false;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	Tool repair crafting definition
	Supported crafting method: CRAFT_METHOD_NORMAL.
	Put two damaged tools into the crafting grid, get one tool back.
	There should only be one crafting definition of this type.
*/
class CraftDefinitionToolRepair: public CraftDefinition
{
public:
	CraftDefinitionToolRepair() = delete;
	CraftDefinitionToolRepair(float additional_wear_);

	virtual ~CraftDefinitionToolRepair() = default;

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements, IGameDef *gamedef) const;

	virtual u64 getHash(CraftHashType type) const { return 2; }

	virtual void initHash(IGameDef *gamedef)
	{
		hash_type = CRAFT_HASH_TYPE_COUNT;
	}

	virtual std::string dump() const;

private:
	// This is a constant that is added to the wear of the result.
	// May be positive or negative, allowed range [-1,1].
	// 1 = new tool is completely broken
	// 0 = simply add remaining uses of both input tools
	// -1 = new tool is completely pristine
	float additional_wear = 0.0f;
};

/*
	A cooking (in furnace) definition
	Supported crafting method: CRAFT_METHOD_COOKING.
*/
class CraftDefinitionCooking: public CraftDefinition
{
public:
	CraftDefinitionCooking() = delete;
	CraftDefinitionCooking(
		const std::string &output_,
		const std::string &recipe_,
		float cooktime_,
		const CraftReplacements &replacements_);

	virtual ~CraftDefinitionCooking() = default;

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements, IGameDef *gamedef) const;

	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

private:
	// Output itemstring
	std::string output;
	// Recipe itemstring
	std::string recipe;
	// Recipe item name
	std::string recipe_name;
	// bool indicating if initHash has been called already
	bool hash_inited = false;
	// Time in seconds
	float cooktime;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	A fuel (for furnace) definition
	Supported crafting method: CRAFT_METHOD_FUEL.
*/
class CraftDefinitionFuel: public CraftDefinition
{
public:
	CraftDefinitionFuel() = delete;
	CraftDefinitionFuel(
		const std::string &recipe_,
		float burntime_,
		const CraftReplacements &replacements_);

	virtual ~CraftDefinitionFuel() = default;

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements, IGameDef *gamedef) const;

	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

private:
	// Recipe itemstring
	std::string recipe;
	// Recipe item name
	std::string recipe_name;
	// bool indicating if initHash has been called already
	bool hash_inited = false;
	// Time in seconds
	float burntime;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	Crafting definition manager
*/
class ICraftDefManager
{
public:
	ICraftDefManager() = default;
	virtual ~ICraftDefManager() = default;

	/**
	 * The main crafting function.
	 *
	 * @param input The input grid.
	 * @param output CraftOutput where the result is placed.
	 * @param output_replacements A vector of ItemStacks where replacements are
	 * placed if they cannot be placed in the input. Replacements can be placed
	 * in the input if the stack of the replaced item has a count of 1.
	 * @param decrementInput If true, consume or replace input items.
	 * @param gamedef
	 * @return true if a result was found, otherwise false.
	 */
	virtual bool getCraftResult(CraftInput &input, CraftOutput &output,
			std::vector<ItemStack> &output_replacements,
			bool decrementInput, IGameDef *gamedef) const=0;

	virtual std::vector<CraftDefinition*> getCraftRecipes(CraftOutput &output,
			IGameDef *gamedef, unsigned limit=0) const=0;

	// Print crafting recipes for debugging
	virtual std::string dump() const=0;
};

class IWritableCraftDefManager : public ICraftDefManager
{
public:
	IWritableCraftDefManager() = default;
	virtual ~IWritableCraftDefManager() = default;

	// The main crafting function
	virtual bool getCraftResult(CraftInput &input, CraftOutput &output,
			std::vector<ItemStack> &output_replacements,
			bool decrementInput, IGameDef *gamedef) const=0;
	virtual std::vector<CraftDefinition*> getCraftRecipes(CraftOutput &output,
			IGameDef *gamedef, unsigned limit=0) const=0;

	virtual bool clearCraftsByOutput(const CraftOutput &output, IGameDef *gamedef) = 0;
	virtual bool clearCraftsByInput(const CraftInput &input, IGameDef *gamedef) = 0;

	// Print crafting recipes for debugging
	virtual std::string dump() const=0;

	// Add a crafting definition.
	// After calling this, the pointer belongs to the manager.
	virtual void registerCraft(CraftDefinition *def, IGameDef *gamedef) = 0;

	// Delete all crafting definitions
	virtual void clear()=0;

	// To be called after all mods are loaded, so that we catch all aliases
	virtual void initHashes(IGameDef *gamedef) = 0;
};

IWritableCraftDefManager* createCraftDefManager();
