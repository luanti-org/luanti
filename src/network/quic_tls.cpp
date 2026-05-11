// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "network/quic_tls.h"

#include <ngtcp2/ngtcp2_crypto_ossl.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "util/base64.h"

namespace con
{

std::string compute_cert_digest(X509 *cert)
{
	if (!cert)
		return {};

	// X509_digest computes the digest over the DER encoding directly
	std::string out(SHA256_DIGEST_LENGTH, '\0');
	unsigned int outlen = 0;
	if (!X509_digest(cert, EVP_sha256(),
			reinterpret_cast<unsigned char *>(out.data()), &outlen)) {
		return {};
	}
	return out;
}

std::string hex_encode(const std::string &raw)
{
	static const char hex[] = "0123456789abcdef";
	std::string out;
	out.reserve(raw.size() * 2);
	for (unsigned char b : raw) {
		out.push_back(hex[b >> 4]);
		out.push_back(hex[b & 0xf]);
	}
	return out;
}

namespace {

// Stateless deleter functors for the OpenSSL objects
template <auto F>
struct OsslDel {
	template <typename T> void operator()(T *p) const {
		if (p) {
			F(p);
		}
	}
};

using X509Ptr = std::unique_ptr<X509, OsslDel<X509_free>>;
using EVPKEYPtr = std::unique_ptr<EVP_PKEY, OsslDel<EVP_PKEY_free>>;
using BIOPtr = std::unique_ptr<BIO, OsslDel<BIO_free>>;

bool is_numeric_host(const std::string &name)
{
	for (char c : name) {
		if (!((c >= '0' && c <= '9') || c == '.' || c == ':'))
			return false;
	}
	return !name.empty();
}

bool add_ext_by_nid(X509 *cert, int nid, const char *value)
{
	X509V3_CTX ctx;
	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
	X509_EXTENSION *ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
	if (!ext)
		return false;
	X509_add_ext(cert, ext, -1);
	X509_EXTENSION_free(ext);
	return true;
}

} // namespace

std::string compute_pem_fingerprint_b64(const std::string &cert_pem_path)
{
	BIOPtr bio(BIO_new_file(cert_pem_path.c_str(), "r"));
	if (!bio)
		return {};
	X509Ptr cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
	if (!cert)
		return {};
	std::string raw = compute_cert_digest(cert.get());
	if (raw.empty())
		return {};
	return base64_encode(raw);
}

// Generates a Ed25519 self-signed certificate valid for 10 years
bool generate_self_signed_cert(const std::string &cert_pem_path,
	const std::string &key_pem_path,
	const std::string &bind_hostname)
{
	EVPKEYPtr pkey(EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519"));
	if (!pkey)
		return false;

	X509Ptr cert(X509_new());
	if (!cert)
		return false;

	// X509 version 3 (0x2)
	X509_set_version(cert.get(), 2);

	// This is a number that is unique per CA. Since it's self-signed, we just
	// choose a random 64-bit positive number.
	int64_t serial = 0;
	if (RAND_bytes(reinterpret_cast<unsigned char *>(&serial), sizeof(serial)) != 1) {
		return false;
	}
	if (serial < 0)
		serial = -serial;
	if (!ASN1_INTEGER_set_int64(X509_get_serialNumber(cert.get()), serial))
		return false;

	X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0);
	// Certificate expires 10 years from now
	X509_gmtime_adj(X509_getm_notAfter(cert.get()), 10L * 365 * 24 * 3600);

	if (X509_set_pubkey(cert.get(), pkey.get()) != 1)
		return false;

	X509_NAME *name = X509_get_subject_name(cert.get());
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
		reinterpret_cast<const unsigned char *>("luanti-quic"), -1, -1, 0);
	// This is just self-signed
	X509_set_issuer_name(cert.get(), name);

	// Set required basic extensions
	add_ext_by_nid(cert.get(), NID_basic_constraints, "critical,CA:FALSE");
	add_ext_by_nid(cert.get(), NID_key_usage, "critical,digitalSignature");
	add_ext_by_nid(cert.get(), NID_ext_key_usage, "serverAuth");
	add_ext_by_nid(cert.get(), NID_subject_key_identifier, "hash");

	// Build a single SAN value with all entries
	std::string san = "DNS:localhost,IP:127.0.0.1,IP:0:0:0:0:0:0:0:1";
	if (!bind_hostname.empty()
			&& bind_hostname != "localhost"
			&& bind_hostname != "127.0.0.1"
			&& bind_hostname != "0.0.0.0"
			&& bind_hostname != "::1"
			&& bind_hostname != "::"
			&& bind_hostname != "::0") {
		san += is_numeric_host(bind_hostname) ? ",IP:" : ",DNS:";
		san += bind_hostname;
	}
	add_ext_by_nid(cert.get(), NID_subject_alt_name, san.c_str());

	// Ed25519 uses EdDSA, which gets nullptr for the digest
	if (X509_sign(cert.get(), pkey.get(), nullptr) == 0) {
		ERR_print_errors_fp(stderr);
		return false;
	}

	// Write the cert and private key to disk
	{
		BIOPtr bio(BIO_new_file(cert_pem_path.c_str(), "w"));
		if (!bio || PEM_write_bio_X509(bio.get(), cert.get()) != 1)
			return false;
	}
	{
		BIOPtr bio(BIO_new_file(key_pem_path.c_str(), "w"));
		if (!bio || PEM_write_bio_PKCS8PrivateKey(bio.get(), pkey.get(),
				nullptr, nullptr, 0, nullptr, nullptr) != 1) {
			return false;
		}
	}
	return true;
}

namespace {

// We attach a VerifyConfig to the SSL object using SSL_set_ex_data.
// To do that, we need to register an index with OpenSSL.
int g_verify_ex_index = -1;

void verify_cfg_free(void *, void *ptr, CRYPTO_EX_DATA *, int, long, void *)
{
	delete static_cast<VerifyConfig *>(ptr);
}

int verify_callback(int preverify_ok, X509_STORE_CTX *store_ctx)
{
	SSL *ssl = static_cast<SSL *>(X509_STORE_CTX_get_ex_data(store_ctx,
		SSL_get_ex_data_X509_STORE_CTX_idx()));
	if (!ssl)
		return 0;

	auto *cfg = static_cast<VerifyConfig *>(SSL_get_ex_data(ssl, g_verify_ex_index));
	if (!cfg)
		return 0;

	// Refuse numeric IP addresses that didn't supply a pinned certificate
	// This is mostly so we can print a more friendly error message
	if (cfg->numeric_no_pin) {
		fprintf(stderr, "QUIC: Connecting to an IP address without a domain requires a "
			"certificate fingerprint. You must add the +<base64 cert fingerprint> to "
			"the address when connecting.\n");
		return 0;
	}

	if (!cfg->pinned_sha256_raw.empty()) {
		// Pinned fingerprint mode: we only have to check the leaf (depth 0)
		if (X509_STORE_CTX_get_error_depth(store_ctx) != 0)
			return 1;

		X509 *leaf = X509_STORE_CTX_get_current_cert(store_ctx);
		std::string actual = compute_cert_digest(leaf);
		if (actual != cfg->pinned_sha256_raw) {
			fprintf(stderr, "QUIC: pinned SHA-256 mismatch\n"
				"  expected: %s\n"
				"  got:      %s\n",
				hex_encode(cfg->pinned_sha256_raw).c_str(),
				hex_encode(actual).c_str());
			return 0;
		}
		return 1;
	}

	// Web PKI mode: rely on the system trust store and hostname verification
	return preverify_ok;
}

} // namespace

TlsClientContext::TlsClientContext() = default;

TlsClientContext::~TlsClientContext()
{
	if (m_ctx)
		SSL_CTX_free(m_ctx);
}

bool TlsClientContext::init()
{
	if (g_verify_ex_index < 0) {
		// Calls verify_cfg_free when the SSL object is freed
		// We only need to register this index once throughout the application lifetime
		g_verify_ex_index = SSL_get_ex_new_index(0, const_cast<char *>("luanti.verify"),
			nullptr, nullptr, verify_cfg_free);
		if (g_verify_ex_index < 0)
			return false;
	}

	m_ctx = SSL_CTX_new(TLS_client_method());
	if (!m_ctx)
		return false;

	SSL_CTX_set_min_proto_version(m_ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(m_ctx, TLS1_3_VERSION);

	// Load the system trust store for the Web PKI fallback path
	if (!SSL_CTX_set_default_verify_paths(m_ctx)) {
		// Not necessarily a fatal error, since pinned fingerprint mode doesn't need this
		ERR_clear_error();
	}

	// We always request VERIFY_PEER and decide to accept or reject in our callback
	SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER, verify_callback);

	return true;
}

bool TlsClientContext::configureSession(SSL *ssl, const VerifyConfig &config)
{
	// Allocate a heap copy which OpenSSL will free via verify_cfg_free when the
	// SSL object is destroyed
	auto cfg = std::make_unique<VerifyConfig>(config);
	if (!SSL_set_ex_data(ssl, g_verify_ex_index, cfg.get()))
		return false;
	cfg.release();

	if (SSL_set_alpn_protos(ssl, LUANTI_ALPN_WIRE, LUANTI_ALPN_WIRE_LEN) != 0)
		return false;

	// SNI and hostname for Web PKI verification (ignored when using a pinned cert)
	if (!config.hostname.empty()) {
		SSL_set_tlsext_host_name(ssl, config.hostname.c_str());
		if (config.pinned_sha256_raw.empty() && !config.numeric_no_pin) {
			SSL_set1_host(ssl, config.hostname.c_str());
			SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
		}
	}

	// SSL object must know it's a client before SSL_do_handshake() runs
	SSL_set_connect_state(ssl);

	// Hand control of the QUIC TLS record path to ngtcp2_crypto_ossl
	return ngtcp2_crypto_ossl_configure_client_session(ssl) == 0;
}

TlsServerContext::TlsServerContext() = default;

TlsServerContext::~TlsServerContext()
{
	if (m_ctx)
		SSL_CTX_free(m_ctx);
}

namespace {

// This is for selecting which protocol to use, via ALPN
// We return the Luanti one if the client listed it, otherwise return error
int alpn_select_cb(SSL *, const unsigned char **out, unsigned char *outlen,
	const unsigned char *in, unsigned int inlen, void *)
{
	// The TLS wire format for ALPN is [length][protocol name...][length][protocol name...]
	for (unsigned int i = 0; i + 1 < inlen;) {
		// Read the protocol name length
		unsigned char alen = in[i];
		if (i + 1 + alen > inlen)
			break;

		// Check if this protocol name matches ours
		if (alen == LUANTI_ALPN_LEN
			&& memcmp(in + i + 1, LUANTI_ALPN, LUANTI_ALPN_LEN) == 0) {
			*out = in + i + 1;
			*outlen = alen;
			return SSL_TLSEXT_ERR_OK;
		}
		i += 1 + alen;
	}
	return SSL_TLSEXT_ERR_ALERT_FATAL;
}

} // namespace

bool TlsServerContext::init(const std::string &cert_pem_path, const std::string &key_pem_path)
{
	m_ctx = SSL_CTX_new(TLS_server_method());
	if (!m_ctx)
		return false;

	SSL_CTX_set_min_proto_version(m_ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(m_ctx, TLS1_3_VERSION);

	if (SSL_CTX_use_certificate_chain_file(m_ctx, cert_pem_path.c_str()) != 1) {
		ERR_print_errors_fp(stderr);
		return false;
	}
	if (SSL_CTX_use_PrivateKey_file(m_ctx, key_pem_path.c_str(),
			SSL_FILETYPE_PEM) != 1) {
		ERR_print_errors_fp(stderr);
		return false;
	}
	if (!SSL_CTX_check_private_key(m_ctx)) {
		ERR_print_errors_fp(stderr);
		return false;
	}

	SSL_CTX_set_alpn_select_cb(m_ctx, alpn_select_cb, nullptr);

	return true;
}

} // namespace con
