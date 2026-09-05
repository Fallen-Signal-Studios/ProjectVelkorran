// Portable tests compile the exact packet/acceptance policy included by production GAS.
#include "GAS/SovCombatTransactionPolicy.h"
#include <cassert>
#include <limits>
#include <iostream>
int main()
{
	using namespace SovCombatTransaction;
	constexpr double Eps = 0.0001;
	assert(BoundedProduct(20., .25) == 5.f);
	assert(BoundedProduct(1e30, 1e30) == std::numeric_limits<float>::max());
	assert(BoundedProduct(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()) == std::numeric_limits<float>::max());
	assert(BoundedProduct(0., std::numeric_limits<double>::max()) == 0.f);
	assert(BoundedProduct(-1., 10.) == 0.f);
	assert(BoundedProduct(std::numeric_limits<double>::quiet_NaN(), 1.) == 0.f);
	assert(BoundedProduct(1., std::numeric_limits<double>::infinity()) == 0.f);
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
	std::cout << "21 combat transaction policy assertions passed\n";
}
