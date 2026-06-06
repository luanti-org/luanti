// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"

#include "network/address.h"
#include "network/networkexceptions.h"
#include "settings.h"
#include "util/base64.h"

#include <string>

namespace {

const std::string pretend_cert_pin = "B0KLp0qT72HeQlIFywNg37sv8elgZTergaqpXKF+hJU=";

// Ensure IPV6 is enabled for the parser
void set_settings()
{
	if (!g_settings)
		return;
	if (!g_settings->exists("enable_ipv6"))
		g_settings->setBool("enable_ipv6", true);
}

} // namespace

TEST_CASE("Address::Resolve parses the QUIC cert pin/host/port", "[quic][address]")
{
	set_settings();

	SECTION("plain numeric IP without pin")
	{
		Address a;
		a.setPort(30000);
		a.Resolve("127.0.0.1");
		CHECK(a.getPort() == 30000);
		CHECK(a.isNumericHostName());
		CHECK_FALSE(a.hasPinnedCertSha256());
		// With a numeric IP address we require the cert pin
		CHECK(a.requiresPinnedCertSha256());
	}

	SECTION("host:port form")
	{
		Address a;
		a.setPort(30001);
		a.Resolve("127.0.0.1");
		CHECK(a.getPort() == 30001);
		CHECK(a.isNumericHostName());
		CHECK_FALSE(a.hasPinnedCertSha256());
	}

	SECTION("host+pin form")
	{
		Address a;
		a.Resolve(("127.0.0.1+" + pretend_cert_pin).c_str());
		CHECK(a.hasPinnedCertSha256());
		CHECK(a.getPinnedCertSha256Base64() == pretend_cert_pin);
		// 32 raw bytes after decode
		CHECK(base64_decode(a.getPinnedCertSha256Base64()).size() == 32);
	}

	SECTION("non-numeric hostname without pin")
	{
		// "localhost" is reliably resolvable
		Address a;
		a.setPort(30000);
		try {
			a.Resolve("localhost");
		} catch (const ResolveError &) {
			SKIP("localhost not resolvable in this environment");
		}
		CHECK(a.getPort() == 30000);
		CHECK_FALSE(a.isNumericHostName());
		CHECK_FALSE(a.hasPinnedCertSha256());
		CHECK(a.shouldUseWebPki());
	}

	SECTION("IPv6 with certificate pin")
	{
		Address a;
		a.Resolve(("::1+" + pretend_cert_pin).c_str());
		CHECK(a.isIPv6());
		CHECK(a.hasPinnedCertSha256());
		CHECK(a.getPinnedCertSha256Base64() == pretend_cert_pin);
	}
}

TEST_CASE("Address::Resolve rejects malformed compositions", "[quic][address]")
{
	set_settings();

	SECTION("base64 that does not decode to 32 bytes")
	{
		Address a;
		CHECK_THROWS_AS(a.Resolve("127.0.0.1+WRONG000"), ResolveError);
	}

	SECTION("invalid base64")
	{
		Address a;
		CHECK_THROWS_AS(a.Resolve("127.0.0.1+not valid base64"), ResolveError);
	}

	SECTION("missing host before pin")
	{
		Address a;
		CHECK_THROWS_AS(a.Resolve(("+" + pretend_cert_pin).c_str()), ResolveError);
	}
}
