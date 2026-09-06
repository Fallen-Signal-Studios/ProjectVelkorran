// Portable tests compile the exact packet/acceptance policy included by production GAS.
#include "GAS/SovCombatTransactionPolicy.h"
#include <cassert>
#include <limits>
#include <iostream>
int main()
{
	using namespace SovCombatTransaction;
	constexpr double Eps = 0.0001;
	assert(SelectPacket(30, 80, true, 1, Eps) == EPacket::Body);
	assert(SelectPacket(0, 80, true, 1, Eps) == EPacket::Poise);
	assert(SelectPacket(0, 0, true, 1, Eps) == EPacket::Control);
	assert(SelectPacket(0, 0, false, 1, Eps) == EPacket::None);
	assert(SelectPacket(0, 0, true, 0, Eps) == EPacket::None);
	assert(SelectPacket(std::numeric_limits<double>::quiet_NaN(), 80, true, 1, Eps) == EPacket::None);
	assert(SelectPacket(0, std::numeric_limits<double>::infinity(), false, 1, Eps) == EPacket::None);
	assert(SelectPacket(0, 0, true, std::numeric_limits<double>::infinity(), Eps) == EPacket::None);
	assert(AcceptStatusRequest(true, false, false, true, 0, Eps));
	assert(!AcceptStatusRequest(true, false, true, true, 0, Eps));
	assert(!AcceptStatusRequest(true, true, false, true, 0, Eps));
	assert(!AcceptStatusRequest(true, true, true, false, 20, Eps));
	assert(AcceptStatusRequest(true, false, true, false, 20, Eps));
	assert(!AcceptStatusRequest(false, false, false, true, 0, Eps));
	assert(BoundedProduct(30, 2) == 60.f);
	assert(BoundedProduct(0, 2) == 0.f);
	assert(BoundedProduct(-1, 2) == 0.f);
	assert(BoundedProduct(1, std::numeric_limits<double>::infinity()) == 0.f);
	assert(BoundedProduct(std::numeric_limits<double>::quiet_NaN(), 1) == 0.f);
	assert(BoundedProduct(1e30, 1e30) == std::numeric_limits<float>::max());
	assert(BoundedProduct(std::numeric_limits<double>::max(), std::numeric_limits<double>::max())
		== std::numeric_limits<float>::max());
	assert(BoundedProduct(std::numeric_limits<float>::max(), 1) == std::numeric_limits<float>::max());
	assert(BoundedProduct(1e-30, 1e30) == 1.f);
	FCallbackBudget Budget;
	unsigned Accepted = 0, Deepest = 0;
	const auto BranchingCallback = [&](auto&& Recurse) -> void
	{
		FScopedCallbackBudget Scope(Budget);
		if (!Scope.IsAdmitted()) { return; }
		++Accepted;
		if (Budget.Depth > Deepest) { Deepest = Budget.Depth; }
		Recurse(Recurse);
		Recurse(Recurse);
	};
	BranchingCallback(BranchingCallback);
	assert(Accepted == 128); // Not an exponentially large depth-32 callback tree.
	assert(Deepest == 32);
	assert(Budget.Depth == 0);
	assert(Budget.Remaining == 0);
	{
		FScopedCallbackBudget Fresh(Budget);
		assert(Fresh.IsAdmitted());
		assert(Budget.Remaining == 127);
	}
	assert(Budget.Depth == 0);
	std::cout << "30 combat transaction policy assertions passed\n";
}
