/* IBE libary function prototypes
 * names are almost self-explanatory
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#ifndef IBE_H
#define IBE_H

#ifndef __KERNEL__
#include <stdio.h>
#endif

#include "curve.h"
#include "byte_string.h"

struct params_s {
    char *id;
    char *version;
    mpz_t p, q;
    point_t P;
    point_t Ppub;

    curve_t curve;

    int sharet, sharen;
    mpz_t *robustx;
    point_t *robustP;

    //can be derived from other parameters
    mpz_t p1onq;
    fp2_t zeta; //cube root of unity
    point_t PhiPpub;
    miller_cache_t Ppub_mc;
};

typedef struct params_s params_t[1];

void IBE_init(void); //initialize library
void IBE_clear(void); //call when done with library

void params_out(FILE *outfp, params_t params); //print system parameters
void params_clear(params_t params); //call when done with params
void params_robust_clear(params_t params); //only free the fields
    //associated with secret splitting (need this for debugging)

//The four basic operations; see paper

void IBE_setup(params_t params, byte_string_t master,
	int k, int qk, char *system);
//generate system parameters

void IBE_extract(byte_string_t key,
	byte_string_t master, const char *id, params_t params);
//extract private key corresponding to given ID
//shouldn't normally be used because the master shouldn't be known
//by only one server

void IBE_split_master(byte_string_t *mshare,
	byte_string_t master, int t, int n, params_t params);
//split the master key into n pieces
//t of them are required to generate a key

int IBE_construct_master(byte_string_t master,
	byte_string_t *mshare, params_t params);
//reconstruct the master from master shares
//(shouldn't normally be used since they should never be in one place)

int IBE_serialize_params(byte_string_t bs, params_t params);
//put system parameters into a byte_string
int IBE_deserialize_params(params_t params, byte_string_t bs);
//get system parameters from a byte_string

void IBE_extract_share(byte_string_t share,
	byte_string_t master_share, const char *id, params_t params);
//extract a private key share from an ID and master share

int IBE_combine(byte_string_t key, byte_string_t *kshare, params_t params);
//reconstruct a key from key shares
//also reconstructs a certificate from certificate shares

int IBE_threshold(params_t params);
int IBE_sharecount(params_t params);
char* IBE_system(params_t params);
char* IBE_version(params_t params);

//for anonymous IBE:

//we now use the Boneh-Franklin system as a Key Encapsulation Mechanism
//a KEM is like a public-key cryptosystem, except encrypt produces
//an encryption of a random secret, rather than the encryption of a given message
//we use "U" to denote the encryption of the randomly generated secret
//one version is for multiple recipients
//more efficient to batch them because the encryption is the same

void IBE_KEM_encrypt_array(byte_string_t *secret, byte_string_t U,
    char **idarray, int count, params_t params);
//for each ID in an array, generate a random key for it, and its encryption U
//(every ID receives the same U, but it decrypts to something unique for them)

void IBE_KEM_encrypt(byte_string_t secret,
	byte_string_t U, char *id, params_t params);
//single ID version

void IBE_KEM_decrypt(byte_string_t secret,
	byte_string_t U, byte_string_t key, params_t params);
//decrypt U to recover the secret

//The following functions are wrappers around the above functions.
//We use the secret produced from the KEM to encrypt another key K
//then we use K to encrypt the actual message
//The reason for this is that if a big message has n recipients,
//rather than encrypting the message n times with n different secrets,
//we encrypt it once with K, and then instead encrypt K n times with
//n different secrets. (K is guaranteed to be small.)

int IBE_hide_key_array(byte_string_t U, byte_string_t *V,
	char **id, int idcount, byte_string_t K, params_t params);
//for each ID,
//generate a random secret and its encryption U
//and set V = encryption of the given key K using the random secret

int IBE_hide_key(byte_string_t U, byte_string_t V,
	char *id, byte_string_t K, params_t params);
//single ID version

int IBE_reveal_key(byte_string_t K,
	byte_string_t U, byte_string_t V, byte_string_t privkey, params_t params);
//decrypt K from a given U and V

//for authenticated IBE:

void IBE_get_shared_secret(byte_string_t s,
	char *id, byte_string_t key, params_t params);
//compute the secret s shared between the holder of the private key "key" and
//the holder of the private key corresponding to the public key "id"


struct preprocessed_key_s {
    miller_cache_t mc;
};

//using preprocessing is slower the first time, but faster every subsequent time
//preprocessing happens automatically for anonymous IBE
//for authenticated IBE, its up to the application
typedef struct preprocessed_key_s preprocessed_key_t[1];
void preprocessed_key_init(preprocessed_key_t pk, params_t params);
void preprocessed_key_clear(preprocessed_key_t pk);

void IBE_get_shared_secret_preprocess(preprocessed_key_t pk,
	byte_string_t key, params_t params);
void IBE_get_shared_secret_postprocess(byte_string_t s,
	char *id, preprocessed_key_t pk, params_t params);

//Boneh-Lynn-Shacham signature routines
//uses same system parameters as IBE

void BLS_keygen(byte_string_t privkey, byte_string_t pubkey, params_t params);
//generate private and public key pair

void BLS_sign(byte_string_t sig,
	byte_string_t message, byte_string_t key, params_t params);
//sign a message

int BLS_verify(byte_string_t sig,
	byte_string_t message, byte_string_t pubkey, params_t params);
//verify a signature

//We use BLS signatures to do identity-based signatures, by using certificates
//Does signature aggregation to compress the certificate chain

void IBE_keygen(byte_string_t privkey, byte_string_t pubkey, params_t params);
//generate private and public key pair

void IBE_certify_share(byte_string_t share, byte_string_t mshare,
	byte_string_t pubkey, const char *id, params_t params);
//extract a certificate share from an ID, public key, and master share
//use IBE_combine to construct a full certificate from certificate shares

void IBE_certify(byte_string_t cert, byte_string_t master,
	byte_string_t pubkey, const char *id, params_t params);
//compute certificate directly from the master key (for testing)

void IBE_sign(byte_string_t sig, byte_string_t message, byte_string_t privkey,
	byte_string_t cert, params_t params);
//sign a message, needs a certificate
//uses signature aggregation
//i.e. sig is actually the signature and the certificate
//note: must send public key to recipient along with message and signature

int IBE_verify(byte_string_t sig, byte_string_t message, byte_string_t pubkey,
	const char *id, params_t params);
//verify a signature

#endif //IBE_H
