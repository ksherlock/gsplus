#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define zeromem(a,b) memset(a,0,b)
#define XMEMCPY memcpy
#define XMEMSET memset

#define LTC_MUTEX_PROTO(x)
#define LTC_ARGCHK(x)

#define LTC_NO_FILE
#define LTC_NO_PROTOTYPES

#define LTC_MD2
#define LTC_MD4
#define LTC_MD5
#define LTC_SHA1
#define LTC_SHA3

#define LTC_BLAKE2S
#define LTC_BLAKE2B


/* error codes [will be expanded in future releases] */
enum {
   CRYPT_OK=0,             /* Result OK */
   CRYPT_ERROR,            /* Generic Error */
   CRYPT_NOP,              /* Not a failure but no operation was performed */

   CRYPT_INVALID_KEYSIZE,  /* Invalid key size given */
   CRYPT_INVALID_ROUNDS,   /* Invalid number of rounds */
   CRYPT_FAIL_TESTVECTOR,  /* Algorithm failed test vectors */

   CRYPT_BUFFER_OVERFLOW,  /* Not enough space for output */
   CRYPT_INVALID_PACKET,   /* Invalid input packet given */

   CRYPT_INVALID_PRNGSIZE, /* Invalid number of bits for a PRNG */
   CRYPT_ERROR_READPRNG,   /* Could not read enough from PRNG */

   CRYPT_INVALID_CIPHER,   /* Invalid cipher specified */
   CRYPT_INVALID_HASH,     /* Invalid hash specified */
   CRYPT_INVALID_PRNG,     /* Invalid PRNG specified */

   CRYPT_MEM,              /* Out of memory */

   CRYPT_PK_TYPE_MISMATCH, /* Not equivalent types of PK keys */
   CRYPT_PK_NOT_PRIVATE,   /* Requires a private PK key */

   CRYPT_INVALID_ARG,      /* Generic invalid argument */
   CRYPT_FILE_NOTFOUND,    /* File Not Found */

   CRYPT_PK_INVALID_TYPE,  /* Invalid type of PK key */

   CRYPT_OVERFLOW,         /* An overflow of a value was detected/prevented */

   CRYPT_UNUSED1,          /* UNUSED1 */
   CRYPT_UNUSED2,          /* UNUSED2 */

   CRYPT_PK_INVALID_SIZE,  /* Invalid size input for PK parameters */

   CRYPT_INVALID_PRIME_SIZE,/* Invalid size of prime requested */
   CRYPT_PK_INVALID_PADDING, /* Invalid padding on input */

   CRYPT_HASH_OVERFLOW      /* Hash applied to too many bits */
};

//#include "blake2.h"

#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_hash.h"
