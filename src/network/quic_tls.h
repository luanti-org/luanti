// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <memory>
#include <string>

namespace con
{

// Our own ALPN identifier
constexpr const char *LUANTI_ALPN = "luanti";
constexpr unsigned char LUANTI_ALPN_WIRE[] = {6, 'l', 'u', 'a', 'n', 't', 'i'};
constexpr size_t LUANTI_ALPN_LEN = LUANTI_ALPN_WIRE[0];
constexpr size_t LUANTI_ALPN_WIRE_LEN = sizeof(LUANTI_ALPN_WIRE);

struct VerifyConfig {
	// If non-empty, the certificate's SHA-256 fingerprint must equal this 32 byte raw hash
	std::string pinned_sha256_raw;
	// Hostname for SNI/Web PKI verification (when certificate pinning is not used)
	std::string hostname;
	// True when the address was a numeric IP but no certificate fingerprint was set
	bool numeric_no_pin = false;
};

std::string compute_cert_digest(X509 *cert);
std::string hex_encode(const std::string &raw);

// Returns an empty string on failure
std::string compute_pem_fingerprint_b64(const std::string &cert_pem_path);

// Generate a new self-signed Ed25519 certificate and key pair, writing to
// cert_pem_path and key_pem_path. The certificate has CN "luanti-quic"
// and SubjectAltName entries for "localhost" and the bind hostname, if it's not empty.
// Returns true on success.
bool generate_self_signed_cert(const std::string &cert_pem_path,
	const std::string &key_pem_path,
	const std::string &bind_hostname = "");

class TlsClientContext
{
public:
	TlsClientContext();
	~TlsClientContext();

	// Returns true on success
	bool init();

	// Configure verification policy for a single connection
	bool configureSession(SSL *ssl, const VerifyConfig &config);

	SSL_CTX *get() const { return m_ctx; }

private:
	SSL_CTX *m_ctx = nullptr;
};

class TlsServerContext
{
public:
	TlsServerContext();
	~TlsServerContext();

	// Loads a PEM-encoded certificate and private key
	bool init(const std::string &cert_pem_path, const std::string &key_pem_path);

	SSL_CTX *get() const { return m_ctx; }

private:
	SSL_CTX *m_ctx = nullptr;
};

} // namespace con
