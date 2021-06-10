#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "cipher/cipher.h"

#define INFOFILE_RSA_DECRYPTION 	1
#define SHA_HASH_LENGTH             56
#define RSA_SIGNING_LENGTH          256

/*
unsigned char []="-----BEGIN PUBLIC KEY-----\n"\
                             "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAy8Dbv8prpJ/0kKhlGeJY\n"\
                             "ozo2t60EG8L0561g13R29LvMR5hyvGZlGJpmn65+A4xHXInJYiPuKzrKUnApeLZ+\n"\
                             "vw1HocOAZtWK0z3r26uA8kQYOKX9Qt/DbCdvsF9wF8gRK0ptx9M6R13NvBxvVQAp\n"\
                             "fc9jB9nTzphOgM4JiEYvlV8FLhg9yZovMYd6Wwf3aoXK891VQxTr/kQYoq1Yp+68\n"\
                             "i6T4nNq7NWC+UNVjQHxNQMQMzU6lWCX8zyg3yH88OAQkUXIXKfQ+NkvYQ1cxaMoV\n"\
                             "PpY72+eVthKzpMeyHkBn7ciumk5qgLTEJAfWZpe4f4eFZj/Rc8Y8Jj2IS5kVPjUy\n"\
                             "wQIDAQAB\n"\
                             "-----END PUBLIC KEY-----\n";
							 */


char rd_rsa_buffer[RSA_SIGNING_LENGTH] = {0, };
char rd_sha_temp[SHA_HASH_LENGTH+1] = {0, };

int get_rsa_parsing(char *file_path)
{
	int rfd;
	struct stat buf;
	int read_buf_size;
	int readCnt = 0;
	char rsa_buffer[RSA_SIGNING_LENGTH] = {0, };
	memset(&buf, 0, sizeof(struct stat));

	rfd = open(file_path, O_RDONLY);
	if(rfd < 0)
	{
		printf("File Open Error : %s !!\n", file_path);
		close(rfd);
		return -1;
	}

	if(fstat(rfd, &buf) < 0)
	{
		printf("cannot read file info\n");
		return -1;
	}
	read_buf_size = buf.st_size +1;

	readCnt = read(rfd, rsa_buffer, read_buf_size);
	if (readCnt < 0)
	{
		printf("read failed with return value: %ld\n",readCnt);
		return -1;
	}
	//printf("==> rsa_buffer = %s\n",rsa_buffer);
	memcpy(rd_rsa_buffer, rsa_buffer, sizeof(rsa_buffer));
	printf("==> rd_rsa_buffer = %s\n",rd_rsa_buffer);

	close(rfd);

	return 0;;
}

RSA *createRSA(unsigned char *key, int dec_type)
{
	RSA *rsa= NULL;
	BIO *keybio ;
	keybio = BIO_new_mem_buf(key, -1);
	if (keybio==NULL)
	{
		printf( "Failed to create key BIO");
		return 0;
	}
	if(dec_type)
	{
		rsa = PEM_read_bio_RSA_PUBKEY(keybio, &rsa,NULL, NULL);
	}
	else
	{
		rsa = PEM_read_bio_RSAPrivateKey(keybio, &rsa,NULL, NULL);
	}
	if(rsa == NULL)
	{
		printf( "Failed to create RSA");
	}

	return rsa;
}

int rec_check_tcc_RSA(char *enc_data)
{
	unsigned char  encrypted[RSA_SIGNING_LENGTH]={0,};
	unsigned char decrypted[SHA_HASH_LENGTH+1]={0,};
	unsigned char mBuf[512] = {0x0,};
	int decrypted_length = 0;
	int padding = RSA_PKCS1_PADDING;
	int ret = 0;
	RSA *rsa = NULL;

	ret = read_public_key_otp(mBuf,512);
	if (ret < 0) {
		return -1;
	}

	rsa = createRSA(mBuf,1);

	if (rsa == NULL) {
		printf("[ERROR] create RSA error \r\n");
		return -2;
	}

	memcpy(encrypted, enc_data, RSA_SIGNING_LENGTH);
	decrypted_length = RSA_public_decrypt(RSA_SIGNING_LENGTH, encrypted, decrypted, rsa, padding);
	if(decrypted_length == -1)
	{
		printf("fail decrypt!!\n");
		return -3;
	}

	strncpy(rd_sha_temp, (char *)decrypted, SHA_HASH_LENGTH);

	return 0;
}

int rsa_hash_compare(char *file_path, char *info_path)
{
	int ret = 0;
	char rsa_buffer[RSA_SIGNING_LENGTH] = {0, };
	char sha_temp[RSA_SIGNING_LENGTH] = {0, };
	char sha224_h[58] = {0,};

	ret = get_rsa_parsing(file_path);

	printf("==> get_rsa_parsing() ret = %d\n",ret);

	ret = rec_check_tcc_RSA(rd_rsa_buffer);

	memset(&sha224_h,0x00,sizeof(char)*58);
	ret = Run_SHA224_hash(info_path, (char *)sha224_h);
	if(ret < 0)
	{
		printf("get hash error !!\n");
		return -1;
	}
	else
	{
		printf("===> jason : sha224_hash = %s\n",sha224_h);
		if(strncmp((char const *)rd_sha_temp, (char const *)sha224_h, SHA_HASH_LENGTH) == 0)
		{
			printf("RSA <=> SHA224 matched !!\n");
			return 0;
		}
		else
		{
			printf("RSA <=> SHA224 mis-matched !!\n");
			return -1;
		}
	}
	return 0;
}
