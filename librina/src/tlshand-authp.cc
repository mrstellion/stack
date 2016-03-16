//
// TLS Handshake authentication policy
//
//    Eduard Grasa <eduard.grasa@i2cat.net>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA  02110-1301  USA
//

#include <time.h>
#include <cstdlib>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/ssl.h>
#include <openssl/md5.h>


#define RINA_PREFIX "librina.tls-handshake"

#include "librina/logs.h"
#include "librina/tlshand-authp.h"
#include "auth-policies.pb.h"

namespace rina {

//TLSHandAuthOptions encoder and decoder operations
void decode_tls_hand_auth_options(const ser_obj_t &message,
		TLSHandAuthOptions &options)
{
	rina::auth::policies::googleprotobuf::authOptsTLSHandshake_t gpb_options;

	gpb_options.ParseFromArray(message.message_, message.size_);

	for(int i=0; i<gpb_options.cipher_suites_size(); i++) {
		options.cipher_suites.push_back(gpb_options.cipher_suites(i));
	}

	for(int i=0; i<gpb_options.compress_methods_size(); i++) {
		options.compress_methods.push_back(gpb_options.compress_methods(i));
	}
	for(int i=0; i<gpb_options.encrypt_algs_size(); i++) {
		options.encrypt_algs.push_back(gpb_options.encrypt_algs(i));
	}
	options.random.utc_unix_time = gpb_options.utc_unix_time();

	if (gpb_options.has_random_bytes()) {
		options.random.random_bytes.data =
				new unsigned char[gpb_options.random_bytes().size()];
		memcpy(options.random.random_bytes.data,
				gpb_options.random_bytes().data(),
				gpb_options.random_bytes().size());
		options.random.random_bytes.length = gpb_options.random_bytes().size();
	}
}

void encode_tls_hand_auth_options(const TLSHandAuthOptions& options,
		ser_obj_t& result)
{
	rina::auth::policies::googleprotobuf::authOptsTLSHandshake_t gpb_options;

	for(std::list<std::string>::const_iterator it = options.cipher_suites.begin();
			it != options.cipher_suites.end(); ++it) {
		gpb_options.add_cipher_suites(*it);
	}

	for(std::list<std::string>::const_iterator it = options.compress_methods.begin();
			it != options.compress_methods.end(); ++it) {
		gpb_options.add_compress_methods(*it);
	}
	for(std::list<std::string>::const_iterator it = options.encrypt_algs.begin();
			it != options.encrypt_algs.end(); ++it) {
		gpb_options.add_encrypt_algs(*it);
	}

	gpb_options.set_utc_unix_time(options.random.utc_unix_time);

	if (options.random.random_bytes.length > 0) {
		gpb_options.set_random_bytes(options.random.random_bytes.data,
				options.random.random_bytes.length);
	}

	int size = gpb_options.ByteSize();
	result.message_ = new unsigned char[size];
	result.size_ = size;
	gpb_options.SerializeToArray(result.message_, size);
}

void encode_server_hello_tls_hand(const TLSHandRandom& random,
		const std::string& cipher_suite,
		const std::string& compress_method,
		const std::string& version,
		ser_obj_t& result)
{
	rina::auth::policies::googleprotobuf::serverHelloTLSHandshake_t gpb_hello;

	gpb_hello.set_random_bytes(random.random_bytes.data,
			random.random_bytes.length);
	gpb_hello.set_utc_unix_time(random.utc_unix_time);
	gpb_hello.set_version(version);
	gpb_hello.set_cipher_suite(cipher_suite);
	gpb_hello.set_compress_method(compress_method);

	int size = gpb_hello.ByteSize();
	result.message_ = new unsigned char[size];
	result.size_ = size;
	gpb_hello.SerializeToArray(result.message_ , size);
}



void decode_server_hello_tls_hand(const ser_obj_t &message,
		TLSHandRandom& random,
		std::string& cipher_suite,
		std::string& compress_method,
		std::string& version)
{
	rina::auth::policies::googleprotobuf::serverHelloTLSHandshake_t gpb_hello;

	gpb_hello.ParseFromArray(message.message_, message.size_);

	if (gpb_hello.has_random_bytes()) {
		random.random_bytes.data =
				new unsigned char[gpb_hello.random_bytes().size()];
		memcpy(random.random_bytes.data,
				gpb_hello.random_bytes().data(),
				gpb_hello.random_bytes().size());
		random.random_bytes.length = gpb_hello.random_bytes().size();
	}

	random.utc_unix_time = gpb_hello.utc_unix_time();
	version = gpb_hello.version();
	cipher_suite = gpb_hello.cipher_suite();
	compress_method = gpb_hello.compress_method();
}


//Certificates
void encode_certificate_tls_hand(const UcharArray& certificate_chain,
		ser_obj_t& result)
{
	rina::auth::policies::googleprotobuf::CertificateTLSHandshake_t gpb_certificate;

	gpb_certificate.set_certificate_chain(certificate_chain.data, certificate_chain.length);

	int size = gpb_certificate.ByteSize();
	result.message_ = new unsigned char[size];
	result.size_ = size;
	gpb_certificate.SerializeToArray(result.message_ , size);
}

void decode_certificate_tls_hand(const ser_obj_t &message,
		 UcharArray& certificate_chain)
{
	rina::auth::policies::googleprotobuf::CertificateTLSHandshake_t gpb_certificate;

	gpb_certificate.ParseFromArray(message.message_, message.size_);

	if (gpb_certificate.has_certificate_chain()) {
		certificate_chain.data =  new unsigned char[gpb_certificate.certificate_chain().size()];
		memcpy(certificate_chain.data,
				gpb_certificate.certificate_chain().data(),
				gpb_certificate.certificate_chain().size());
		certificate_chain.length = gpb_certificate.certificate_chain().size();
	}
}

//Client key_exchange
void encode_client_key_exchange_tls_hand(const UcharArray& enc_pmaster_secret,
		ser_obj_t& result)
{
	rina::auth::policies::googleprotobuf::clientKeyExchangeTLSHandshake_t gpb_key_exchange;

	gpb_key_exchange.set_enc_pmaster_secret(enc_pmaster_secret.data, enc_pmaster_secret.length);

	int size = gpb_key_exchange.ByteSize();
	result.message_ = new unsigned char[size];
	result.size_ = size;
	gpb_key_exchange.SerializeToArray(result.message_ , size);
}

void decode_client_key_exchange_tls_hand(const ser_obj_t &message,
		 UcharArray& enc_pmaster_secret)
{
	rina::auth::policies::googleprotobuf::clientKeyExchangeTLSHandshake_t gpb_key_exchange;

	gpb_key_exchange.ParseFromArray(message.message_, message.size_);

		if (gpb_key_exchange.has_enc_pmaster_secret()) {
			enc_pmaster_secret.data =  new unsigned char[gpb_key_exchange.enc_pmaster_secret().size()];
			memcpy(enc_pmaster_secret.data,
					gpb_key_exchange.enc_pmaster_secret().data(),
					gpb_key_exchange.enc_pmaster_secret().size());
			enc_pmaster_secret.length = gpb_key_exchange.enc_pmaster_secret().size();
		}
}

//Client certificate verify
void encode_client_certificate_verify_tls_hand(const UcharArray& enc_verify_hash,
		ser_obj_t& result)
{
	rina::auth::policies::googleprotobuf::clientCertificateVerifyTLSHandshake_t gpb_cert_verify;

	gpb_cert_verify.set_enc_verify_hash(enc_verify_hash.data, enc_verify_hash.length);

	int size = gpb_cert_verify.ByteSize();
	result.message_ = new unsigned char[size];
	result.size_ = size;
	gpb_cert_verify.SerializeToArray(result.message_ , size);
}
void decode_client_certificate_verify_tls_hand(const ser_obj_t &message,
		 UcharArray& enc_verify_hash)
{
	rina::auth::policies::googleprotobuf::clientCertificateVerifyTLSHandshake_t gpb_cert_verify;

	gpb_cert_verify.ParseFromArray(message.message_, message.size_);

		if (gpb_cert_verify.has_enc_verify_hash()) {
			enc_verify_hash.data =  new unsigned char[gpb_cert_verify.enc_verify_hash().size()];
			memcpy(enc_verify_hash.data,
					gpb_cert_verify.enc_verify_hash().data(),
					gpb_cert_verify.enc_verify_hash().size());
			enc_verify_hash.length = gpb_cert_verify.enc_verify_hash().size();
		}
}

//Finish messages
void encode_finish_message_tls_hand(const UcharArray& opaque_verify_data,
		ser_obj_t& result)
{
	rina::auth::policies::googleprotobuf::FinishMessageTLSHandshake_t gpb_finish;

	gpb_finish.set_opaque_verify_data(opaque_verify_data.data, opaque_verify_data.length);

	int size = gpb_finish.ByteSize();
	result.message_ = new unsigned char[size];
	result.size_ = size;
	gpb_finish.SerializeToArray(result.message_ , size);
}
void decode_finsih_message_tls_hand(const ser_obj_t &message,
		UcharArray& opaque_verify_data)
{
	rina::auth::policies::googleprotobuf::FinishMessageTLSHandshake_t gpb_finish;

	gpb_finish.ParseFromArray(message.message_, message.size_);

	if (gpb_finish.has_opaque_verify_data()) {
		opaque_verify_data.data =  new unsigned char[gpb_finish.opaque_verify_data().size()];
		memcpy(opaque_verify_data.data,
				gpb_finish.opaque_verify_data().data(),
				gpb_finish.opaque_verify_data().size());
		opaque_verify_data.length = gpb_finish.opaque_verify_data().size();
	}
}


// Class TLSHandSecurityContext
const std::string TLSHandSecurityContext::CIPHER_SUITE = "cipherSuite";
const std::string TLSHandSecurityContext::COMPRESSION_METHOD = "compressionMethod";
const std::string TLSHandSecurityContext::KEYSTORE_PATH = "keystore";
const std::string TLSHandSecurityContext::KEYSTORE_PASSWORD = "keystorePass";
const std::string TLSHandSecurityContext::CERTIFICATE_PATH = "myCredentials";
const std::string TLSHandSecurityContext::MY_CERTIFICATE = "certificate.pem";
const std::string TLSHandSecurityContext::PRIV_KEY_PATH = "myPrivKey";
const std::string TLSHandSecurityContext::ENCRYPTION_ALGORITHM = "encryptAlg";

TLSHandSecurityContext::~TLSHandSecurityContext()
{
	if (cert) {
		X509_free(cert);
		cert = NULL;
	}
}

CryptoState TLSHandSecurityContext::get_crypto_state(bool enable_crypto_tx,
		bool enable_crypto_rx)
{
	CryptoState result;
	result.enable_crypto_tx = enable_crypto_tx;
	result.enable_crypto_rx = enable_crypto_rx;
	//TODO
	result.encrypt_key_tx = encrypt_key;
	result.port_id = id;

	return result;
}


TLSHandSecurityContext::TLSHandSecurityContext(int session_id,
		const AuthSDUProtectionProfile& profile)
: ISecurityContext(session_id)
{
	cipher_suite = profile.authPolicy.get_param_value_as_string(CIPHER_SUITE);
	compress_method = profile.authPolicy.get_param_value_as_string(COMPRESSION_METHOD);
	keystore_path = profile.authPolicy.get_param_value_as_string(KEYSTORE_PATH);
	if (keystore_path == std::string()) {
		//TODO set the configuration directory as the default keystore path
	}
	keystore_password = profile.authPolicy.get_param_value_as_string(KEYSTORE_PASSWORD);
	crcPolicy = profile.crcPolicy;
	ttlPolicy = profile.ttlPolicy;
	encrypt_policy_config = profile.encryptPolicy;
	con.port_id = session_id;

	encrypt_alg = profile.encryptPolicy.get_param_value_as_string(ENCRYPTION_ALGORITHM);

	certificate_path = profile.authPolicy.get_param_value_as_string(CERTIFICATE_PATH);
	priv_key_path = profile.authPolicy.get_param_value_as_string(PRIV_KEY_PATH);

	timer_task = NULL;
	state = BEGIN;

	cert = NULL;
	other_cert = NULL;
	cert_received = false;
	hello_received = false;
	client_cert_received = false;
	client_keys_received = false;
	client_cert_verify_received = false;
	client_cipher_received = false;
	master_secret.length = 48;
	master_secret.data = new unsigned char[48];
	verify_data.length = 12;
	verify_data.data = new unsigned char[12];
}

TLSHandSecurityContext::TLSHandSecurityContext(int session_id,
		const AuthSDUProtectionProfile& profile,
		TLSHandAuthOptions * options)
: ISecurityContext(session_id)
{
	std::string option = options->cipher_suites.front();
	if (option != "TODO") {
		LOG_ERR("Unsupported cipher suite: %s",
				option.c_str());
		throw Exception();
	} else {
		cipher_suite = option;
	}

	option = options->compress_methods.front();
	if (option != "TODO") {
		LOG_ERR("Unsupported compression method: %s",
				option.c_str());
		throw Exception();
	} else {
		compress_method = option;
	}

	option = options->encrypt_algs.front();
	if (option != SSL_TXT_AES128 && option != SSL_TXT_AES256) {
		LOG_ERR("Unsupported encryption algorithm: %s",
				option.c_str());
		throw Exception();
	} else {
		encrypt_alg = option;
	}


	client_random = options->random;

	certificate_path = profile.authPolicy.get_param_value_as_string(CERTIFICATE_PATH);
	priv_key_path = profile.authPolicy.get_param_value_as_string(PRIV_KEY_PATH);

	keystore_path = profile.authPolicy.get_param_value_as_string(KEYSTORE_PATH);
	if (keystore_path == std::string()) {
		//TODO set the configuration directory as the default keystore path
	}
	keystore_password = profile.authPolicy.get_param_value_as_string(KEYSTORE_PASSWORD);
	crcPolicy = profile.crcPolicy;
	ttlPolicy = profile.ttlPolicy;
	encrypt_policy_config = profile.encryptPolicy;
	con.port_id = session_id;
	timer_task = NULL;
	cert = NULL;
	other_cert = NULL;
	cert_received = false;
	hello_received = false;
	client_cert_received = false;
	client_keys_received = false;
	client_cert_verify_received = false;
	client_cipher_received = false;
	master_secret.length = 48;
	master_secret.data = new unsigned char[48];
	verify_data.length = 12;
	verify_data.data = new unsigned char[12];


	state = BEGIN;
}

//Class AuthTLSHandPolicySet
const int AuthTLSHandPolicySet::DEFAULT_TIMEOUT = 10000;
const std::string AuthTLSHandPolicySet::SERVER_HELLO = "Server Hello";
const std::string AuthTLSHandPolicySet::SERVER_CERTIFICATE = "Server Certificate";
const std::string AuthTLSHandPolicySet::CLIENT_CERTIFICATE = "Client Certificate";
const std::string AuthTLSHandPolicySet::CLIENT_KEY_EXCHANGE = "Client key exchange";
const std::string AuthTLSHandPolicySet::CLIENT_CERTIFICATE_VERIFY = "Client certificate verify";
const std::string AuthTLSHandPolicySet::CLIENT_CHANGE_CIPHER_SPEC = "Client change cipher spec";
const std::string AuthTLSHandPolicySet::SERVER_CHANGE_CIPHER_SPEC = "Server change cipher spec";
const std::string AuthTLSHandPolicySet::CLIENT_FINISH = "Client finish";
const std::string AuthTLSHandPolicySet::SERVER_FINISH = "Server finish";



AuthTLSHandPolicySet::AuthTLSHandPolicySet(rib::RIBDaemonProxy * ribd,
		ISecurityManager * sm) :
				IAuthPolicySet(IAuthPolicySet::AUTH_TLSHAND)
{
	rib_daemon = ribd;
	sec_man = sm;
	timeout = DEFAULT_TIMEOUT;
}

AuthTLSHandPolicySet::~AuthTLSHandPolicySet()
{
}

cdap_rib::auth_policy_t AuthTLSHandPolicySet::get_auth_policy(int session_id,
							      const cdap_rib::ep_info_t& peer_ap,
							      const AuthSDUProtectionProfile& profile)
{
	(void) peer_ap;

	if (profile.authPolicy.name_ != type) {
		LOG_ERR("Wrong policy name: %s, expected: %s",
				profile.authPolicy.name_.c_str(),
				type.c_str());
		throw Exception();
	}

	ScopedLock sc_lock(lock);

	if (sec_man->get_security_context(session_id) != 0) {
		LOG_ERR("A security context already exists for session_id: %d",
				session_id);
		throw Exception();
	}

	LOG_DBG("Initiating authentication for session_id: %d", session_id);
	cdap_rib::auth_policy_t auth_policy;
	auth_policy.name = IAuthPolicySet::AUTH_TLSHAND;
	auth_policy.versions.push_back(profile.authPolicy.version_);

	TLSHandSecurityContext * sc = new TLSHandSecurityContext(session_id,
			profile);
	sc->client_random.utc_unix_time = (unsigned int) time(NULL);
	sc->client_random.random_bytes.data = new unsigned char[28];
	sc->client_random.random_bytes.length = 28;
	if (RAND_bytes(sc->client_random.random_bytes.data,
			sc->client_random.random_bytes.length) == 0) {
		LOG_ERR("Problems generating client random bytes: %s",
				ERR_error_string(ERR_get_error(), NULL));
		delete sc;
		throw Exception();
	}

	TLSHandAuthOptions options;
	options.cipher_suites.push_back(sc->cipher_suite);
	options.compress_methods.push_back(sc->compress_method);
	options.random = sc->client_random;
	options.encrypt_algs.push_back(sc->encrypt_alg);

	encode_tls_hand_auth_options(options, auth_policy.options);

	//Store security context
	sc->state = TLSHandSecurityContext::WAIT_SERVER_HELLO_and_CERTIFICATE;
	sec_man->add_security_context(sc);

	//Initialized verify hash, used in certificate verify message
	sc->verify_hash.data = new unsigned char[32*5];
	sc->verify_hash.length = 32*5;
	sc->master_secret.length = 48;
	sc->master_secret.data = new unsigned char[48];

	//Get auth policy options to obtain first hash message [0,--31]
	unsigned char hash1[SHA256_DIGEST_LENGTH];
	if(!SHA256(auth_policy.options.message_, auth_policy.options.size_, hash1)){
		LOG_ERR("Could not has message");
		throw Exception();
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data, hash1, 32);

	return auth_policy;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::initiate_authentication(const cdap_rib::auth_policy_t& auth_policy,
									 const AuthSDUProtectionProfile& profile,
									 const cdap_rib::ep_info_t& peer_ap,
									 int session_id)
{
	(void) peer_ap;

	if (auth_policy.name != type) {
		LOG_ERR("Wrong policy name: %s", auth_policy.name.c_str());
		return IAuthPolicySet::FAILED;
	}

	if (auth_policy.versions.front() != RINA_DEFAULT_POLICY_VERSION) {
		LOG_ERR("Unsupported policy version: %s",
				auth_policy.versions.front().c_str());
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sec_man->get_security_context(session_id) != 0) {
		LOG_ERR("A security context already exists for session_id: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	LOG_DBG("Initiating authentication for session_id: %d", session_id);
	TLSHandAuthOptions options;
	decode_tls_hand_auth_options(auth_policy.options, options);

	TLSHandSecurityContext * sc;
	try {
		sc = new TLSHandSecurityContext(session_id, profile, &options);
	} catch (Exception &e){
		return IAuthPolicySet::FAILED;
	}

	//Initialized verify hash, used in certificate verify message
	sc->verify_hash.data = new unsigned char[32*5];
	sc->verify_hash.length = 32*5;
	sc->master_secret.length = 48;
	sc->master_secret.data = new unsigned char[48];
	//Get auth policy options to obtain first hash message [0,--31]
	unsigned char hash1[SHA256_DIGEST_LENGTH];
	if(!SHA256(auth_policy.options.message_, auth_policy.options.size_, hash1)){
		LOG_ERR("Coul not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data, hash1, 32);

	//Generate server random
	sc->server_random.utc_unix_time = (unsigned int) time(NULL);
	sc->server_random.random_bytes.data = new unsigned char[28];
	sc->server_random.random_bytes.length = 28;
	if (RAND_bytes(sc->server_random.random_bytes.data,
			sc->server_random.random_bytes.length) == 0) {
		LOG_ERR("Problems generating server random bytes: %s",
				ERR_error_string(ERR_get_error(), NULL));
		delete sc;
		return IAuthPolicySet::FAILED;
	}

	//Send Server Hello
	cdap_rib::obj_info_t obj_info;
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		//cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = SERVER_HELLO;
		obj_info.name_ = SERVER_HELLO;
		obj_info.inst_ = 0;
		encode_server_hello_tls_hand(sc->server_random,
				sc->cipher_suite,
				sc->compress_method,
				RINA_DEFAULT_POLICY_VERSION,
				obj_info.value_);

		rib_daemon->remote_write(sc->con, obj_info, flags, filt, NULL);

	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s", e.what());
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	timer.scheduleTask(sc->timer_task, timeout);

	//Get onj.info_value to obtain second hash message [32,--64]
	unsigned char hash2[SHA256_DIGEST_LENGTH];
	if(!SHA256(obj_info.value_.message_, obj_info.value_.size_, hash2)){
		LOG_ERR("Could not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+32, hash2, 32);

	load_authentication_certificate(sc);
	//convert x509
	UcharArray encoded_cert;
	encoded_cert.length = i2d_X509(sc->cert, &encoded_cert.data);
	if (encoded_cert.length < 0)
		LOG_ERR("Error converting certificate");

	//Send server certificate
	cdap_rib::obj_info_t obj_info1;
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		//cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info1.class_ = SERVER_CERTIFICATE;
		obj_info1.name_ = SERVER_CERTIFICATE;
		obj_info1.inst_ = 0;
		encode_certificate_tls_hand(encoded_cert,
				obj_info1.value_);

		rib_daemon->remote_write(sc->con,
				obj_info1,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",
				e.what());
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	sc->state = TLSHandSecurityContext::WAIT_CLIENT_CERTIFICATE_and_KEYS;
	sec_man->add_security_context(sc);

	timer.scheduleTask(sc->timer_task, timeout);

	//Get obj.info_value to obtain third hash message [64,--96]
	unsigned char hash3[SHA256_DIGEST_LENGTH];
	if(!SHA256(obj_info1.value_.message_, obj_info1.value_.size_, hash3)){
		LOG_ERR("Could not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+64, hash3, 32);

	return IAuthPolicySet::IN_PROGRESS;
}

int AuthTLSHandPolicySet::process_incoming_message(const cdap::CDAPMessage& message,
		int session_id)
{
	if (message.op_code_ != cdap::CDAPMessage::M_WRITE) {
		LOG_ERR("Wrong operation type");
		return IAuthPolicySet::FAILED;
	}

	if (message.obj_class_ == SERVER_HELLO) {
		return process_server_hello_message(message, session_id);
	}
	if (message.obj_class_ == SERVER_CERTIFICATE) {
		return process_server_certificate_message(message, session_id);
	}
	if (message.obj_class_ == CLIENT_CERTIFICATE) {
		return process_client_certificate_message(message, session_id);
	}
	if (message.obj_class_ == CLIENT_KEY_EXCHANGE) {
		return process_client_key_exchange_message(message, session_id);
	}
	if (message.obj_class_ == CLIENT_CERTIFICATE_VERIFY) {
		return process_client_certificate_verify_message(message, session_id);
	}
	if (message.obj_class_ == CLIENT_CHANGE_CIPHER_SPEC) {
		return process_client_change_cipher_spec_message(message, session_id);
	}
	if (message.obj_class_ == SERVER_CHANGE_CIPHER_SPEC) {
		return process_server_change_cipher_spec_message(message, session_id);
	}
	if (message.obj_class_ == CLIENT_FINISH) {
		return process_client_finish_message(message, session_id);
	}
	if (message.obj_class_ == SERVER_FINISH) {
		return process_server_finish_message(message, session_id);
	}

	return rina::IAuthPolicySet::FAILED;
}

int AuthTLSHandPolicySet::load_authentication_certificate(TLSHandSecurityContext * sc)
{
	BIO * certstore;
	std::stringstream ss;

	ss << sc->certificate_path.c_str() << "/" << TLSHandSecurityContext::MY_CERTIFICATE;

	certstore =  BIO_new_file(ss.str().c_str(),  "r");
	if (!certstore) {
		LOG_ERR("Problems opening certificate file at: %s", ss.str().c_str());
		return -1;
	}
	sc->cert = PEM_read_bio_X509(certstore, NULL, 0, NULL);
	BIO_free(certstore);
	if (!sc->cert) {
		LOG_ERR("Problems reading certificate %s", ERR_error_string(ERR_get_error(), NULL));
		return -1;

	}
	return rina::IAuthPolicySet::IN_PROGRESS;
}

int AuthTLSHandPolicySet::prf(UcharArray& generated_hash, UcharArray& secret,  const std::string& slabel, UcharArray& pre_seed)
{
	//convert label to UcharArray
	UcharArray label(slabel.length());
	memcpy(label.data, slabel.c_str(), slabel.length());

	//compute how many times we need to hask a(i)
	int it = (generated_hash.length/32);
	if (generated_hash.length%32 != 0)  it+=1;

	std::vector<UcharArray> vec(it+1);
	std::vector<UcharArray> vres(it+1);

	//calculate seed, v(0) = seed;
	UcharArray seed(label, pre_seed);
	vec[0].length=seed.length;
	vec[0].data = new unsigned char[seed.length];
	memcpy(vec[0].data, seed.data, seed.length);

	//compute a[i], for determined length and second hmac call
	for(int i = 1; i <= it; ++i){
		vec[i].length = 32;
		vec[i].data = new unsigned char[32];
		HMAC(EVP_sha256(),secret.data, secret.length, vec[i-1].data, vec[i-1].length, vec[i].data, (unsigned *)(&vec[i].length));
		if(vec[i].data == NULL)LOG_ERR("Error calculating master secret");

		UcharArray X0(vec[i], vec[0]);
		vres[i].length = 32;
		vres[i].data = new unsigned char[32];
		HMAC(EVP_sha256(),secret.data, secret.length, X0.data, X0.length, vres[i].data, (unsigned *)(&vres[i].length));
		if(vres[i].data == NULL)LOG_ERR("Error calculating master secret");
	}
	UcharArray con(it*32);
	if(it == 1) memcpy(generated_hash.data, vres[1].data, generated_hash.length);
	//Concatenate and get desired length
	else {
		for(int i = 1; i <= it-1; ++i){
			UcharArray concatenate(vres[i], vres[i+1]);
			memcpy(con.data+((i-1)*concatenate.length), concatenate.data, concatenate.length);

		}
		memcpy(generated_hash.data, con.data, generated_hash.length);
	}
	return IAuthPolicySet::IN_PROGRESS;

}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_server_hello_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;

	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::WAIT_SERVER_HELLO_and_CERTIFICATE) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	sc->hello_received = true;

	decode_server_hello_tls_hand(message.obj_value_,
			sc->server_random,
			sc->cipher_suite,
			sc->compress_method,
			sc->version);

	timer.cancelTask(sc->timer_task);

	//Get obj.info_value options to obtain third hash message [0,--31]
	unsigned char hash2[SHA256_DIGEST_LENGTH];
	if(!SHA256(message.obj_value_.message_, message.obj_value_.size_, hash2)){
		LOG_ERR("Could not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+32, hash2, 32);


	//if certificate received change state
	if(sc->cert_received) {
		sc->state = TLSHandSecurityContext::CLIENT_SENDING_DATA;
		return send_client_messages(sc);

	}
	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_server_certificate_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;

	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::WAIT_SERVER_HELLO_and_CERTIFICATE) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	sc->cert_received = true;

	UcharArray certificate_chain;
	decode_certificate_tls_hand(message.obj_value_,certificate_chain);
	timer.cancelTask(sc->timer_task);

	//hash3 to concatenate for verify message
	unsigned char hash3[SHA256_DIGEST_LENGTH];
	if(!SHA256(message.obj_value_.message_, message.obj_value_.size_, hash3)){
		LOG_ERR("Could not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+64, hash3, 32);

	const unsigned char *aux;
	aux =  reinterpret_cast<const unsigned char*>(certificate_chain.data);
	const unsigned char** pointer;
	pointer = &aux;

	//get certificate in x509 format
	if(pointer ==NULL)
		LOG_ERR("Bad pointer :(");
	sc->other_cert = d2i_X509(NULL, pointer, certificate_chain.length);
	if(sc->other_cert  == NULL)
		LOG_ERR("Bad conversion to x509 :(");

	if(sc->hello_received) {
		sc->state = TLSHandSecurityContext::CLIENT_SENDING_DATA;
		return send_client_messages(sc);
	}
	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_client_certificate_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;

	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::WAIT_CLIENT_CERTIFICATE_and_KEYS) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	UcharArray certificate_chain;
	decode_certificate_tls_hand(message.obj_value_,certificate_chain);
	timer.cancelTask(sc->timer_task);

	sc->client_cert_received = true;

	//preparation for certificate verify message
	unsigned char hash4[SHA256_DIGEST_LENGTH];
	if(!SHA256(message.obj_value_.message_, message.obj_value_.size_, hash4)){
		LOG_ERR("Could not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+96, hash4, 32);

	//Transform certificate to x509 format and store it
	const unsigned char *aux;
	aux =  reinterpret_cast<const unsigned char*>(certificate_chain.data);
	const unsigned char** pointer;
	pointer = &aux;

	if(pointer ==NULL)
		LOG_ERR("Bad pointer :(");
	sc->other_cert = d2i_X509(NULL, pointer, certificate_chain.length);
	if(sc->other_cert  == NULL)
		LOG_ERR("Bad conversion to x509 :(");

	if(sc->client_keys_received and sc->client_cert_verify_received and sc->client_cipher_received){
		sc->state = TLSHandSecurityContext::SERVER_SENDING_CIPHER;
		return send_server_change_cipher_spec(sc);
	}
	return IAuthPolicySet::IN_PROGRESS;

}
IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_client_key_exchange_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;

	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::WAIT_CLIENT_CERTIFICATE_and_KEYS) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}

	UcharArray enc_pre_master_secret;
	decode_client_key_exchange_tls_hand(message.obj_value_, enc_pre_master_secret);
	sc->client_keys_received = true;
	timer.cancelTask(sc->timer_task);

	//preparation for certificate verify message
	unsigned char hash5[SHA256_DIGEST_LENGTH];
	if(!SHA256(message.obj_value_.message_, message.obj_value_.size_, hash5)){
		LOG_ERR("Could not hash message");
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+128, hash5, 32);

	EVP_PKEY *privkey = NULL;
	RSA *rsakey;
	BIO *key;

	key =  BIO_new_file(sc->priv_key_path.c_str(), "r");
	if (!key) {
		LOG_ERR("Problems opening key file at: %s",sc->priv_key_path.c_str());
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}

	privkey = PEM_read_bio_PrivateKey(key, NULL, 0, NULL);
	BIO_free(key);

	if (!privkey) {
		LOG_ERR("Problems reading  key",ERR_error_string(ERR_get_error(), NULL));
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}

	rsakey = EVP_PKEY_get1_RSA(privkey);
	if(rsakey == NULL)
		LOG_ERR("EVP_PKEY_get1_RSA: failed.");

	UcharArray dec_pre_master_secret;
	dec_pre_master_secret.data = new unsigned char[256];

	if((dec_pre_master_secret.length =  RSA_private_decrypt(enc_pre_master_secret.length,
								enc_pre_master_secret.data,
								dec_pre_master_secret.data,
								rsakey,
								RSA_PKCS1_OAEP_PADDING)) == -1){
		LOG_ERR("Error decrypting pre-master secret");
		ERR_load_crypto_strings();
		ERR_error_string(ERR_get_error(), NULL);
	}

	//EVP_PKEY_free(privkey); //do we need it?

	//start computing MASTERSECRET
	std::string slabel = "master secret";
	UcharArray pre_seed(sc->client_random.random_bytes, sc->server_random.random_bytes);
	//Generate master secret
	prf(sc->master_secret,dec_pre_master_secret, slabel, pre_seed);
	generate_encryption_key(sc);

	if(sc->client_cert_received and sc->client_cert_verify_received and sc->client_cipher_received){
		sc->state = TLSHandSecurityContext::SERVER_SENDING_CIPHER;
		return send_server_change_cipher_spec(sc);
	}
	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_client_certificate_verify_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;

	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::WAIT_CLIENT_CERTIFICATE_and_KEYS) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}

	UcharArray enc_verify_hash;
	decode_client_certificate_verify_tls_hand(message.obj_value_, enc_verify_hash);
	sc->client_cert_verify_received = true;
	timer.cancelTask(sc->timer_task);

	UcharArray dec_verify_hash;
	EVP_PKEY *pubkey = NULL;
	RSA *rsa_pubkey = NULL;

	if ((pubkey = X509_get_pubkey(sc->other_cert)) == NULL)
		LOG_ERR("Error getting public key from certificate %s",
				ERR_error_string(ERR_get_error(), NULL));

	rsa_pubkey = EVP_PKEY_get1_RSA(pubkey);

	if(rsa_pubkey == NULL)
		LOG_ERR("EVP_PKEY_get1_RSA: failed. %s",
				ERR_error_string(ERR_get_error(), NULL));

	dec_verify_hash.data = new unsigned char[RSA_size(rsa_pubkey)];

	if((dec_verify_hash.length = RSA_public_decrypt(enc_verify_hash.length,
			enc_verify_hash.data,
			dec_verify_hash.data,
			rsa_pubkey,
			RSA_PKCS1_PADDING)) == -1){
		LOG_ERR("Error decrypting certificate verify");
		LOG_ERR("Error decrypting certificate verify with RSA public key: %s", ERR_error_string(ERR_get_error(), NULL));
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}

	//Compare calculated hash with received decrypted hash, should be the same if ok auth
	if (dec_verify_hash != sc->verify_hash) {
		LOG_ERR("Error authenticating server. Decrypted Hashed cv: %s, cv: %s",
				dec_verify_hash.toString().c_str(),
				sc->verify_hash.toString().c_str());
		return IAuthPolicySet::FAILED;
	}

	if(sc->client_keys_received and sc->client_cert_received and sc->client_cipher_received){
		sc->state = TLSHandSecurityContext::SERVER_SENDING_CIPHER;
		return send_server_change_cipher_spec(sc);
	}

	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_client_change_cipher_spec_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;
	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}
	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::WAIT_CLIENT_CERTIFICATE_and_KEYS) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	timer.cancelTask(sc->timer_task);

	if(!sc->client_keys_received or !sc->client_cert_received or !sc->client_cert_verify_received){
		LOG_ERR("Not enough messages received to proceed...");
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}


	//TODO
	// Configure kernel SDU protection policy with master secret and algorithms
	// tell it to enable decryption
	AuthStatus result = sec_man->update_crypto_state(sc->get_crypto_state(false, true),this);
	if (result == IAuthPolicySet::FAILED) {
		delete sc;
		return result;
	}
	sc->state = TLSHandSecurityContext::REQUESTED_ENABLE_DECRYPTION_SERVER;
	if (result == IAuthPolicySet::SUCCESSFULL) {
		result = decryption_enabled_server(sc);
	}
	return result;
	//fi SDU
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_server_change_cipher_spec_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;
	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}
	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}
	ScopedLock sc_lock(lock);


	if (sc->state != TLSHandSecurityContext::WAIT_SERVER_CIPHER) {
		LOG_ERR("Wrong session state: %d", sc->state);
		LOG_ERR("falla  a process server change");
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}

	timer.cancelTask(sc->timer_task);

	//TODO
	/*// Configure kernel SDU protection policy with master secret and algorithms
	// tell it to enable decryption and encryption*/
	AuthStatus result = sec_man->update_crypto_state(sc->get_crypto_state(true, true),this);
	if (result == IAuthPolicySet::FAILED) {
		sec_man->destroy_security_context(sc->id);
		return result;
	}

	sc->state = TLSHandSecurityContext::REQUESTED_ENABLE_ENCRYPTION_DECRYPTION_CLIENT;
	if (result == IAuthPolicySet::SUCCESSFULL) {
		result = encryption_decryption_enabled_client(sc);
	}
	return result;

}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_client_finish_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;
	LOG_DBG("entroooo process client finsih!!!!!!");
	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::SERVER_SENDING_CIPHER) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}


	UcharArray dec_client_finish;
	decode_finsih_message_tls_hand(message.obj_value_,dec_client_finish);


	//Calculate my finish prf
	std::string slabel ="finish label";
	prf(sc->verify_data,sc->master_secret, slabel, sc->verify_hash);

	if (dec_client_finish != sc->verify_data) {
		LOG_ERR("Error authenticating server. Decrypted finish: %s, stored finish: %s",
				dec_client_finish.toString().c_str(),
				sc->verify_data.toString().c_str());

		return IAuthPolicySet::FAILED;
	}

	LOG_DBG("before cryptoseeerver!!!!!!");
	timer.cancelTask(sc->timer_task);
	//SDU
	//TODO
	// Configure kernel SDU protection policy with master secret and algorithms
	// tell it to enable encryption
	AuthStatus result = sec_man->update_crypto_state(sc->get_crypto_state(true, true),this);
	if (result == IAuthPolicySet::FAILED) {
		delete sc;
		return result;
	}
	sc->state = TLSHandSecurityContext::REQUESTED_ENABLE_ENCRYPTION_SERVER;
	if (result == IAuthPolicySet::SUCCESSFULL) {
		result = encryption_enabled_server(sc);
	}
	return result;

}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::process_server_finish_message(const cdap::CDAPMessage& message,
		int session_id)
{
	TLSHandSecurityContext * sc;

	if (message.obj_value_.message_ == 0) {
		LOG_ERR("Null object value");
		return IAuthPolicySet::FAILED;
	}

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(session_id));
	if (!sc) {
		LOG_ERR("Could not retrieve Security Context for session: %d", session_id);
		return IAuthPolicySet::FAILED;
	}

	ScopedLock sc_lock(lock);

	if (sc->state != TLSHandSecurityContext::WAIT_SERVER_FINISH) {
		LOG_ERR("Wrong session state: %d", sc->state);
		LOG_DBG("peta a process server_finish");
		sec_man->remove_security_context(session_id);
		delete sc;
		return IAuthPolicySet::FAILED;
	}
	UcharArray dec_server_finish;
	decode_finsih_message_tls_hand(message.obj_value_, dec_server_finish);

	if (dec_server_finish != sc->verify_data) {
		LOG_ERR("Error authenticating server. Decrypted finish: %s, stored finish: %s",
				dec_server_finish.toString().c_str(),
				sc->verify_data.toString().c_str());

		return IAuthPolicySet::FAILED;
	}
	timer.cancelTask(sc->timer_task);
	sc->state = TLSHandSecurityContext::DONE;
	return IAuthPolicySet::SUCCESSFULL;
}

IAuthPolicySet::AuthStatus  AuthTLSHandPolicySet::send_client_certificate(TLSHandSecurityContext * sc)
{

	load_authentication_certificate(sc);
	//convert x509
	UcharArray encoded_cert;
	encoded_cert.length = i2d_X509(sc->cert, &encoded_cert.data);
	if (encoded_cert.length < 0)
		LOG_ERR("Error converting certificate");

	//Send client certificate
	cdap_rib::obj_info_t obj_info;
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		//cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = CLIENT_CERTIFICATE;
		obj_info.name_ = CLIENT_CERTIFICATE;
		obj_info.inst_ = 0;
		encode_certificate_tls_hand(encoded_cert,
				obj_info.value_);

		rib_daemon->remote_write(sc->con,
				obj_info,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",
				e.what());
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}
	timer.scheduleTask(sc->timer_task, timeout);

	//compute hash for certificate verify message
	unsigned char hash4[SHA256_DIGEST_LENGTH];
	if(!SHA256(obj_info.value_.message_, obj_info.value_.size_, hash4)){
		LOG_ERR("Could not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+96, hash4, 32);
	return IAuthPolicySet::IN_PROGRESS;

}

IAuthPolicySet::AuthStatus  AuthTLSHandPolicySet::send_client_key_exchange(TLSHandSecurityContext * sc)
{

	UcharArray pre_master_secret, enc_pre_master_secret;
	pre_master_secret.data = new unsigned char[48];
	pre_master_secret.length = 48;

	EVP_PKEY *pubkey = NULL;
	RSA *rsa_pubkey = NULL;

	if(RAND_bytes(pre_master_secret.data, pre_master_secret.length) != 1)
		LOG_ERR("Problems generating random bytes");

	//Exctract public key
	if ((pubkey = X509_get_pubkey(sc->other_cert)) == NULL)
		LOG_ERR("Error getting public key from certificate %s",
				ERR_error_string(ERR_get_error(), NULL));

	rsa_pubkey = EVP_PKEY_get1_RSA(pubkey);

	if(rsa_pubkey == NULL)
		LOG_ERR("EVP_PKEY_get1_RSA: failed. %s",
				ERR_error_string(ERR_get_error(), NULL));

	enc_pre_master_secret.data = new unsigned char[RSA_size(rsa_pubkey)];

	if((enc_pre_master_secret.length = RSA_public_encrypt(pre_master_secret.length,
								pre_master_secret.data,
								enc_pre_master_secret.data,
								rsa_pubkey,
								RSA_PKCS1_OAEP_PADDING)) == -1){
		LOG_ERR("Error encrypting pre-master secret");
		LOG_ERR("Error encrypting challenge with RSA public key: %s", ERR_error_string(ERR_get_error(), NULL));
		return IAuthPolicySet::FAILED;
	}


	//es necessari??? free pkey
	/*EVP_PKEY_free(pubkey);
	RSA_free(rsa_pubkey);*/

	//Send client key exchange
	cdap_rib::obj_info_t obj_info;
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		//cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = CLIENT_KEY_EXCHANGE;
		obj_info.name_ = CLIENT_KEY_EXCHANGE;
		obj_info.inst_ = 0;
		encode_client_key_exchange_tls_hand(enc_pre_master_secret,
				obj_info.value_);

		rib_daemon->remote_write(sc->con,
				obj_info,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",e.what());
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}
	timer.scheduleTask(sc->timer_task, timeout);

	//preparation for certificate verify message
	unsigned char hash5[SHA256_DIGEST_LENGTH];
	if(!SHA256(obj_info.value_.message_, obj_info.value_.size_, hash5)){
		LOG_ERR("Could not hash message");
		return IAuthPolicySet::FAILED;
	}
	//prepare verify_hash vector for posterior signing
	memcpy(sc->verify_hash.data+128, hash5, 32);

	//calculate_master_secret
	std::string slabel = "master secret";
	UcharArray pre_seed(sc->client_random.random_bytes, sc->server_random.random_bytes);
	prf(sc->master_secret,pre_master_secret, slabel, pre_seed);

	//calculate encryption key;
	generate_encryption_key(sc);

	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus  AuthTLSHandPolicySet::send_client_certificate_verify(TLSHandSecurityContext * sc)
{

	RSA *rsa_priv_key;
	BIO *key;

	key =  BIO_new_file(sc->priv_key_path.c_str(), "r");
	if (!key) {
		LOG_ERR("Problems opening key file at: %s",sc->priv_key_path.c_str());
		return IAuthPolicySet::FAILED;
	}
	rsa_priv_key = PEM_read_bio_RSAPrivateKey(key, NULL, 0, NULL);
	BIO_free(key);

	if (!rsa_priv_key) {
		LOG_ERR("Problems reading  key",ERR_error_string(ERR_get_error(), NULL));
		return IAuthPolicySet::FAILED;
	}

	//encrypt the hash of all mesages with private rsa key of IPCP A
	UcharArray enc_cert_verify(256);
	if((enc_cert_verify.length = RSA_private_encrypt(sc->verify_hash.length,
							sc->verify_hash.data,
							enc_cert_verify.data,
							rsa_priv_key,
							RSA_PKCS1_PADDING)) == -1){
		LOG_ERR("Error encrypting certificate verify hash");
		LOG_ERR("Error encrypting certificate verify hash with private key: %s", ERR_error_string(ERR_get_error(), NULL));
		return IAuthPolicySet::FAILED;
	}


	//Send client key exchange
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = CLIENT_CERTIFICATE_VERIFY;
		obj_info.name_ = CLIENT_CERTIFICATE_VERIFY;
		obj_info.inst_ = 0;
		encode_client_certificate_verify_tls_hand(enc_cert_verify,
				obj_info.value_);

		rib_daemon->remote_write(sc->con,
				obj_info,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",e.what());
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}

	timer.scheduleTask(sc->timer_task, timeout);
	return IAuthPolicySet::IN_PROGRESS;

}
IAuthPolicySet::AuthStatus  AuthTLSHandPolicySet::send_client_change_cipher_spec(TLSHandSecurityContext * sc)
{
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = CLIENT_CHANGE_CIPHER_SPEC;
		obj_info.name_ = CLIENT_CHANGE_CIPHER_SPEC;
		obj_info.inst_ = 0;
		obj_info.value_.size_ = 1;
		obj_info.value_.message_ = new unsigned char[1];

		rib_daemon->remote_write(sc->con,
				obj_info,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",e.what());
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}
	timer.scheduleTask(sc->timer_task, timeout);
	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::send_client_messages(TLSHandSecurityContext * sc)
{

	if (sc->state != TLSHandSecurityContext::CLIENT_SENDING_DATA) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}

	send_client_certificate(sc);
	send_client_key_exchange(sc);
	send_client_certificate_verify(sc);
	send_client_change_cipher_spec(sc);
	sc->state = TLSHandSecurityContext::WAIT_SERVER_CIPHER;

	LOG_DBG("fienvio 4 missatges cleint");

	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus  AuthTLSHandPolicySet::send_server_change_cipher_spec(TLSHandSecurityContext * sc)
{
	if (sc->state != TLSHandSecurityContext::SERVER_SENDING_CIPHER) {
		LOG_ERR("Wrong session state: %d", sc->state);
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}

	//Send server change cipher spec
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = SERVER_CHANGE_CIPHER_SPEC;
		obj_info.name_ = SERVER_CHANGE_CIPHER_SPEC;
		obj_info.inst_ = 0;
		obj_info.value_.size_ = 1;
		obj_info.value_.message_ = new unsigned char[1];

		rib_daemon->remote_write(sc->con,
				obj_info,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",e.what());
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}
	timer.scheduleTask(sc->timer_task, timeout);
	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus  AuthTLSHandPolicySet::send_client_finish(TLSHandSecurityContext * sc)
{
	if (sc->state != TLSHandSecurityContext::WAIT_SERVER_FINISH) {
		LOG_ERR("Wrong session state: %d", sc->state);
		LOG_DBG("falla a send client function");
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}

	std::string slabel ="finish label";
	prf(sc->verify_data,sc->master_secret, slabel, sc->verify_hash);

	//Send client finish message
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = CLIENT_FINISH;
		obj_info.name_ = CLIENT_FINISH;
		obj_info.inst_ = 0;
		encode_finish_message_tls_hand(sc->verify_data,
				obj_info.value_);

		rib_daemon->remote_write(sc->con,
				obj_info,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",e.what());
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}

	timer.scheduleTask(sc->timer_task, timeout);
	sc->state = TLSHandSecurityContext::WAIT_SERVER_FINISH;
	LOG_DBG("after sending client finish");
	return IAuthPolicySet::IN_PROGRESS;
}

int AuthTLSHandPolicySet::set_policy_set_param(const std::string& name,
		const std::string& value)
{
	LOG_DBG("No policy-set-specific parameters to set (%s, %s)",
			name.c_str(), value.c_str());
	return -1;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::crypto_state_updated(int port_id)
{
	TLSHandSecurityContext * sc;

	ScopedLock sc_lock(lock);

	sc = dynamic_cast<TLSHandSecurityContext *>(sec_man->get_security_context(port_id));
	if (!sc) {
		LOG_ERR("Could not retrieve TLS Handshake security context for port-id: %d",
				port_id);
		return IAuthPolicySet::FAILED;
	}

	switch(sc->state) {
	case TLSHandSecurityContext::REQUESTED_ENABLE_DECRYPTION_SERVER:
		return decryption_enabled_server(sc);
	case TLSHandSecurityContext::REQUESTED_ENABLE_ENCRYPTION_SERVER:
		return encryption_enabled_server(sc);
	case TLSHandSecurityContext::REQUESTED_ENABLE_ENCRYPTION_DECRYPTION_CLIENT:
		return encryption_decryption_enabled_client(sc);
	default:
		LOG_ERR("Wrong security context state: %d", sc->state);
		LOG_ERR("es aqui oi??crypto state update");
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}
}



int AuthTLSHandPolicySet::generate_encryption_key(TLSHandSecurityContext * sc)
{

	if (sc->encrypt_alg == SSL_TXT_AES128) {
		sc->encrypt_key.length = 16;
		sc->encrypt_key.data = new unsigned char[16];
		MD5(sc->master_secret.data, sc->master_secret.length, sc->encrypt_key.data);

	} else if (sc->encrypt_alg == SSL_TXT_AES256) {
		sc->encrypt_key.length = 32;
		sc->encrypt_key.data = new unsigned char[32];
		SHA256(sc->master_secret.data, sc->master_secret.length, sc->encrypt_key.data);
	}

	LOG_DBG("Generated encryption key of length %d bytes: %s",
			sc->encrypt_key.length,
			sc->encrypt_key.toString().c_str());

	return IAuthPolicySet::IN_PROGRESS;
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::decryption_enabled_server(TLSHandSecurityContext * sc)
{
	if (sc->state != TLSHandSecurityContext::REQUESTED_ENABLE_DECRYPTION_SERVER) {
		LOG_ERR("Wrong state of policy");
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}

	LOG_DBG("Decryption enabled for port-id: %d", sc->id);
	sc->state = TLSHandSecurityContext::SERVER_SENDING_CIPHER;
	return send_server_change_cipher_spec(sc);
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::encryption_decryption_enabled_client(TLSHandSecurityContext * sc)
{
	if (sc->state != TLSHandSecurityContext::REQUESTED_ENABLE_ENCRYPTION_DECRYPTION_CLIENT) {
		LOG_ERR("Wrong state of policy");
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}
	LOG_DBG("Encryption and decryption enabled for port-id: %d", sc->id);

	timer.cancelTask(sc->timer_task);
	sc->state = TLSHandSecurityContext::WAIT_SERVER_FINISH;
	return send_client_finish(sc);
}

IAuthPolicySet::AuthStatus AuthTLSHandPolicySet::encryption_enabled_server(TLSHandSecurityContext * sc)
{
	if (sc->state != TLSHandSecurityContext::REQUESTED_ENABLE_ENCRYPTION_SERVER) {
		LOG_ERR("Wrong state of policy");
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}

	LOG_DBG("Encryption enabled for port-id: %d", sc->id);
	//Send server finish message
	LOG_DBG("sending server finish before");
	try {
		cdap_rib::flags_t flags;
		cdap_rib::filt_info_t filt;
		cdap_rib::obj_info_t obj_info;
		cdap::StringEncoder encoder;

		obj_info.class_ = SERVER_FINISH;
		obj_info.name_ = SERVER_FINISH;
		obj_info.inst_ = 0;
		encode_finish_message_tls_hand(sc->verify_data,
				obj_info.value_);

		rib_daemon->remote_write(sc->con,
				obj_info,
				flags,
				filt,
				NULL);
	} catch (Exception &e) {
		LOG_ERR("Problems encoding and sending CDAP message: %s",e.what());
		sec_man->destroy_security_context(sc->id);
		return IAuthPolicySet::FAILED;
	}
	timer.cancelTask(sc->timer_task);
	sc->state = TLSHandSecurityContext::DONE;
	return IAuthPolicySet::SUCCESSFULL;
}

}
