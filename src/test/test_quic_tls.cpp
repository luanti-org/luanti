// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"

#include "network/quic_tls.h"
#include "filesys.h"
#include "util/base64.h"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstdio>
#include <string>

namespace {

struct TempCertPaths {
	std::string dir;
	std::string cert;
	std::string key;

	explicit TempCertPaths(const std::string &tag)
	{
		dir = fs::TempPath() + DIR_DELIM "luanti-quic-test-" + tag;
		fs::CreateAllDirs(dir);
		cert = dir + DIR_DELIM "server.cert.pem";
		key = dir + DIR_DELIM "server.key.pem";
	}
	~TempCertPaths()
	{
		std::remove(cert.c_str());
		std::remove(key.c_str());
		fs::RecursiveDelete(dir);
	}
};

} // namespace

TEST_CASE("generate_self_signed_cert produces usable cert and key pair", "[quic][tls]")
{
	TempCertPaths paths("gen");

	REQUIRE(con::generate_self_signed_cert(paths.cert, paths.key, "localhost"));

	CHECK(fs::IsFile(paths.cert));
	CHECK(fs::IsFile(paths.key));

	std::string cert_pem;
	REQUIRE(fs::ReadFile(paths.cert, cert_pem));
	CHECK(cert_pem.find("BEGIN CERTIFICATE") != std::string::npos);

	const std::string fp_b64 = con::compute_pem_fingerprint_b64(paths.cert);
	REQUIRE_FALSE(fp_b64.empty());
	CHECK(base64_decode(fp_b64).size() == 32);
}

TEST_CASE("Generated cert can be loaded by TlsServerContext", "[quic][tls]")
{
	TempCertPaths paths("server-ctx");
	REQUIRE(con::generate_self_signed_cert(paths.cert, paths.key, ""));

	con::TlsServerContext server_ctx;
	REQUIRE(server_ctx.init(paths.cert, paths.key));
	CHECK(server_ctx.get() != nullptr);
}
