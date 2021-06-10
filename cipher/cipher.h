#ifndef __TELECHIPS_CIPHER_H__
#define __TELECHIPS_CIPHER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NOT_USE						0

#define BLOCK_SIZE					64
#define DIGEST_SIZE					32
#define SHA224_DIGEST_SIZE	        28

#define UPDATE_BUFFER_SIZE 			(5 * 1024 * 1024)

#define MD5_HASH_LENGTH		32
#define SHA1_HASH_LENGTH	40
#define SHA224_HASH_LENGTH	56
#define SHA256_HASH_LENGTH	64
#define SHA384_HASH_LENGTH	96
#define SHA512_HASH_LENGTH	128

#define CIPHER_DEVICE				"/dev/cipher"

#define BYTEORDER_SWAP32(x) (((x) << 24) | (((x) & 0x0000ff00U) << 8) | (((x) & 0x00ff0000U) >> 8) | ((x) >> 24))

#define SHA224_CHECK
#define SHA_BLOCK_SIZE		64
#define SHA_DIGEST_SIZE		28

#define SHA_MAX_BUF_SIZE	(16*1024*1024)
#define SHA_BUF_SIZE		(SHA_MAX_BUF_SIZE - (SHA_BLOCK_SIZE*2))
#define STR_MAX_LENGTH      128
#define REC_UP_FILE_LIST_MAX 64

#define AES_BLOCK_SIZE  			4096
#define REC_AES_BLOCK_SIZE		     16
//#define REC_AES_MAX_BUF_SIZE	     (16*1024*1024)
#define REC_AES_MAX_BUF_SIZE	     (1*1024*1024)
#define REC_AES_BUF_SIZE		     (REC_AES_MAX_BUF_SIZE - REC_AES_BLOCK_SIZE)

#define DEVICE_NAME	"tcc-otp"

typedef struct {
    char f_name_str[STR_MAX_LENGTH];
    char sha224_hash[STR_MAX_LENGTH];
    int cnt;
} rec_up_info_file;

enum tcc_otp_ioctl_cmd {
	TCC_OTP_IOCTL_READ,
	TCC_OTP_IOCTL_WRITE,
	TCC_OTP_IOCTL_MAX
};

struct tcc_otp_ioctl_param {
	unsigned int *addr;
	unsigned int *buf;
	unsigned int size;
};


int cipher_get_sha_hash(const char *filepath, int hash_mode, unsigned char *hash);
int cipher_run_aes_decrypt(const char *filepath, const char *decrypt_filepath);
int rec_info_file_parsing(const char *info_path);
int check_validity_sha224(const char *file_path, const char *file_name);
int check_tcc_AES128(char* enc_input, char* dec_output);
int Run_SHA224_hash(const char * in_file, char *sha224_hash);

int read_public_key_otp(unsigned char *data, int size);
int write_public_key_otp(int *data, int size);

int read_update_aes_key(int *data);
int write_update_aes_key(void);

int set_security_key_to_otp(void);

#ifdef __cplusplus
}
#endif

#endif //__TELECHIPS_CIPHER_H__
