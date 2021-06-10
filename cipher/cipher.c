#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "tcc_cipher.h"
#include "cipher.h"

#define TCC_OTP_DEIVCE_NAME			"/dev/tcc-otp"

rec_up_info_file rec_str_parser[REC_UP_FILE_LIST_MAX];

/*
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
*/

static unsigned char AES_CBC_ucInitVector[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

static void printData(const unsigned char *data, const unsigned int size)
{
	unsigned int i;
	for(i = 0; i < size; i++) {
		printf("%02X", data[i]);
	}
	printf("\n");
}

static int calcPad(const unsigned char data[], const unsigned int size, unsigned char padData[], unsigned int *padSize)
{
	unsigned int i;
	unsigned int pSize = size;

	memcpy(padData, data, size);
	padData[pSize++] = 0x80;
	for (i = (pSize % BLOCK_SIZE); (i > 0) && (i < BLOCK_SIZE); i++)
		padData[pSize++] = 0;

	if ((size % BLOCK_SIZE) > 55) {
		for (i = 0; i < BLOCK_SIZE; i++)
			padData[pSize++] = 0;
	}

	*(unsigned int *)&padData[pSize-4] = BYTEORDER_SWAP32(size*8);

	*padSize = pSize;

	return 0;
}

int cipher_open()
{
	int fd = open(CIPHER_DEVICE, O_RDWR);

	if(fd < 0) {
		printf("[%s] file open error [%s] [%d]\n", __func__, CIPHER_DEVICE, fd); 
		perror("file open failed reason :");
		return -1;
	}

	return fd;
}

int cipher_close(int fd)
{
	if(fd <= 0) {
		printf("[%s] wrong file description [%d]\n", __func__, fd); 
		return -1;
	}

	close(fd);
	
	return 0;
}

int cipher_set_algorithm(int fd, unsigned int mode, unsigned int algorithm, 
						 unsigned int arg1, unsigned int arg2)
{
	int ret;
	stCIPHER_ALGORITHM st_algorithm;

	if(fd <= 0) { 
		printf("[%s] wrong file description [%d]\n", __func__, fd); 
		return -1; 
	}

	if(mode >= TCC_CIPHER_OPMODE_MAX) {
		printf("[%s] wrong mode value [%u]\n", __func__, mode);
		return -1;
	}

	if(mode >= TCC_CIPHER_ALGORITM_MAX) {
		printf("[%s] wrong algorithm value [%u]\n", __func__, algorithm);
		return -1;
	}

	st_algorithm.uOperationMode = mode;
	st_algorithm.uAlgorithm = algorithm;
	st_algorithm.uArgument1 = arg1;
	st_algorithm.uArgument2 = arg2;

	ret = ioctl(fd, TCC_CIPHER_IOCTL_SET_ALGORITHM, &st_algorithm);

	if(ret) {
		printf("[%s] ioctl TCC_CIPHER_IOCTL_SET_ALGORITHM failed [%d]\n", __func__, ret);
		return -1;
	}

	return 0;
}

int cipher_set_key(int fd, const char *key, unsigned int key_length, unsigned int option)
{
	int ret; 
	stCIPHER_KEY st_key;

	if(fd <= 0) { 
		printf("[%s] wrong file description [%d]\n", __func__, fd); 
		return -1; 
	}

	memset(&st_key, 0, sizeof(stCIPHER_KEY));
	memcpy(st_key.pucData, key, key_length);
	st_key.uLength = key_length;
	st_key.uOption = option;

	ret = ioctl(fd, TCC_CIPHER_IOCTL_SET_KEY, &st_key);

	if(ret) {
		printf("[%s] ioctl TCC_CIPHER_IOCTL_SET_KEY failed [%d]\n", __func__, ret);
		return -1;
	}

	return 0;
}

int cipher_set_vector(int fd, const char *vector, unsigned int vector_length)
{
	int ret; 
	stCIPHER_VECTOR st_vector;

	if(fd <= 0) { 
		printf("[%s] wrong file description [%d]\n", __func__, fd); 
		return -1; 
	}

	memset(&st_vector, 0, sizeof(stCIPHER_VECTOR));
	memcpy(st_vector.pucData, vector, vector_length);
	st_vector.uLength = vector_length;

	ret = ioctl(fd, TCC_CIPHER_IOCTL_SET_VECTOR, &st_vector);

	if(ret) {
		printf("[%s] ioctl TCC_CIPHER_IOCTL_SET_KEY failed [%d]\n", __func__, ret);
		return -1;
	}
	
	return 0;
}

int cipher_run_encrypt()
{
	return 0;
}

int cipher_run_decrypt(int fd, unsigned char *src, unsigned char *dst, unsigned int length)
{
	int ret;
	stCIPHER_RUN st_decrypt;

	if(fd <= 0) { 
		printf("[%s] wrong file description [%d]\n", __func__, fd); 
		return -1; 
	}

	if(src == NULL || dst == NULL) {
		printf("[%s] wrong buffer address src [%08X] dst [%08X]\n", 
			   __func__, (unsigned int)src, (unsigned int)dst); 
		return -1; 
	}

	st_decrypt.pucSrcAddr = src;
	st_decrypt.pucDstAddr = dst;
	st_decrypt.uLength = length;

	ret = ioctl(fd, TCC_CIPHER_IOCTL_RUN, &st_decrypt);

	if(ret < 0) {
		printf("[%s] ioctl TCC_CIPHER_IOCTL_RUN failed [%d]\n", __func__, ret);
		return -1;
	}

	return 0;
}

int cipher_get_sha_hash(const char *filepath, int hash_mode, unsigned char *hash)
{
	unsigned char *digest = NULL;
	unsigned char *buffer;
	unsigned char *pad_data;

	unsigned int padSize;
	int ret;

	int target_fd = open(filepath, O_RDONLY);
	if(target_fd < 0) {
		printf("[%s] file open error [%s] [%d]\n", __func__, filepath, target_fd); 
		perror("file open failed reason :");
		return -1;
	}

	struct stat st_stat;
	ret = fstat(target_fd, &st_stat);
	if(ret < 0) {
		printf("[%s] file stat error [%d]\n", __func__, ret); 
		perror("file fstat failed reason :");
		ret = -1;
		goto target_fd_close_exit;
	}

	size_t filesize = st_stat.st_size;
	size_t tempsize = st_stat.st_size + ((filesize % BLOCK_SIZE) ? BLOCK_SIZE - (filesize % BLOCK_SIZE) : 0);

//	printf("[%s] path = [%s] size = [%llu]\n", __func__, filepath, filesize); 

	unsigned int hash_length;
	switch(hash_mode) {
		case MD5:
			hash_length = MD5_HASH_LENGTH;
			break;
		case SHA_1:
			hash_length = SHA1_HASH_LENGTH;
			break;
		case SHA_224:
		case SHA_512_224:
			hash_length = SHA224_HASH_LENGTH;
			break;
		case SHA_256:
		case SHA_512_256:
			hash_length = SHA256_HASH_LENGTH;
			break;
		case SHA_384:
			hash_length = SHA384_HASH_LENGTH;
			break;
		case SHA_512:
			hash_length = SHA512_HASH_LENGTH;
			break;
		default:
			printf("[%s] wrong hash mode [%d]\n", __func__, hash_mode);
			return -1;
	}

	buffer = (unsigned char *)malloc(tempsize);
	pad_data = (unsigned char *)malloc(tempsize);
	digest = (unsigned char *)malloc(hash_length / 2);

	memset(buffer, 0, tempsize);
	memset(pad_data, 0, tempsize);
	memset(digest, 0, hash_length / 2);

	ret = read(target_fd, buffer, filesize);
	if(ret != filesize) {
		printf("[%s] check read function, read filesize [%d], ret [%d]\n", 
			   __func__, filesize, ret);
	}

	calcPad(buffer, filesize, pad_data, &padSize);
//	printf("[%s] pad size = [%u]\n", padSize);

	int cipher_fd = cipher_open();
	if(cipher_fd < 0) {
		printf("[%s] cipher file open error [%d]\n", __func__, cipher_fd); 
		perror("file open failed reason :");
		ret = -1;
		goto alloc_mem_free_exit;
	}

	ret = cipher_set_algorithm(cipher_fd, TCC_CIPHER_OPMODE_ECB, TCC_CIPHER_ALGORITM_HASH,
							   hash_mode, (padSize / BLOCK_SIZE) - 1);
	if(ret < 0) {
		printf("[%s] cipher algorithm setting error [%d]\n", __func__, ret); 
		ret = -1;
		goto cipher_fd_close_exit;
	}

	ret = cipher_run_decrypt(cipher_fd, pad_data, digest, padSize);
	if(ret < 0) {
		printf("[%s] cipher decrypt error [%d]\n", __func__, ret);
		ret = -1;
		goto cipher_fd_close_exit;

	}

	hash = (unsigned char *)malloc(hash_length) + 1;
	if(hash == NULL) {
		printf("[%s] hash memory allocation failed\n", __func__);
		return -1;
	}

	memset(hash, 0, hash_length);
	unsigned int i;
	for(i = 0;i < (hash_length / 2);++i) {
		sprintf((char *)(hash + (i * 2)), "%02X", digest[i]);
	}
	hash[hash_length] = 0;

//	printf("hash [%s]\n", hash);

cipher_fd_close_exit:
	close(cipher_fd);

alloc_mem_free_exit:
	free(buffer);
	free(pad_data);
	free(digest);

target_fd_close_exit:
	close(target_fd);

	return ret;
}

#if 1 //jason.ku 2018.02.26 : add function AES128 Decryption
static int addPad(unsigned char *pBuf, unsigned int uiBufSize)
{
	unsigned char ucPadSize;
	int i;
	
	/* Add PKCS#7 padding on a 16-byte block */
	ucPadSize = REC_AES_BLOCK_SIZE - (uiBufSize % REC_AES_BLOCK_SIZE);

	for(i=0; i<ucPadSize; i++)
	{
		pBuf[uiBufSize+i] = ucPadSize;
	}

	return ucPadSize;
}

static int removePad(unsigned char *pBuf, unsigned int uiBufSize)
{
	unsigned char ucPadSize;
	int i;
	
	/* Remove PKCS#7 padding on a 16-byte block */
	ucPadSize = pBuf[uiBufSize-1];

	for(i=0; i<ucPadSize; i++)
	{
		pBuf[uiBufSize-(i+1)] = 0;
	}

	return ucPadSize;
}

static int setAES128(int hCipher, unsigned char *pVector, int iVectorLen,
	                unsigned char *pKey, int iKeyLen, unsigned char ucEnc)
{
	stCIPHER_ALGORITHM al;
	stCIPHER_KEY key;
	stCIPHER_VECTOR iv;
	stCIPHER_SET set;

	/* set algoritm */
	al.uOperationMode = TCC_CIPHER_OPMODE_CBC;
	al.uAlgorithm = TCC_CIPHER_ALGORITM_AES;
	al.uArgument1 = TCC_CIPHER_KEYLEN_128;
	al.uArgument2 = 0;
	if(ioctl(hCipher, TCC_CIPHER_IOCTL_SET_ALGORITHM, &al)) {
		printf("TCC_CIPHER_IOCTL_SET_ALGORITHM error\n");
		return -1;
	}

	/* vector setting */
	memset(iv.pucData, 0, 16);
	memcpy(iv.pucData, pVector, iVectorLen);
 	iv.uLength = iVectorLen;
	if(ioctl(hCipher, TCC_CIPHER_IOCTL_SET_VECTOR, &iv)) {
		printf("TCC_CIPHER_IOCTL_SET_VECTOR error\n");
		return -1;
	}

	/* key setting */
	memset(key.pucData, 0, 16);
	memcpy(key.pucData, pKey, iKeyLen);
	key.uLength = iKeyLen;
	key.uOption = 0;
	if(ioctl(hCipher, TCC_CIPHER_IOCTL_SET_KEY, &key)) {
		printf("TCC_CIPHER_IOCTL_SET_KEY error\n");
		return -1;
	}

	set.keyLoad = 1;
	set.ivLoad = 1;
	set.enc = ucEnc;
	if(ioctl(hCipher, TCC_CIPHER_IOCTL_SET, &set)) {
		printf("TCC_CIPHER_IOCTL_SET error\n");
		return -1;
	}

	return 0;
}

static int calcAES128(int hCipher, unsigned char *pSrcBuf, unsigned int *puiSrcBufSize,
 	                   unsigned char *pDstBuf, 	unsigned int uiLastFrame)
{
	stCIPHER_RUN run;

	run.pucSrcAddr = pSrcBuf;
	run.pucDstAddr = pDstBuf;
	run.uLength = *puiSrcBufSize;
	run.uPKCS7Pad = uiLastFrame;

	if(ioctl(hCipher, TCC_CIPHER_IOCTL_RUN, &run)) {
		printf("TCC_CIPHER_IOCTL_RUN error\n");
		return -1;
	}

	if(*puiSrcBufSize != run.uLength) {
		printf("Check Pad(%d, %d)\n", *puiSrcBufSize, run.uLength);
	}

	*puiSrcBufSize = run.uLength;

	return 0;
}

static int runAES128(int hCipher,char *pSrcFileName, char *pDstFileName, unsigned char *pSrcBuf,
                	 unsigned char *pDstBuf, unsigned char ucEnc)
{
	FILE *hSrcFile = NULL, *hDstFile = NULL;
	unsigned int uiFileSize = 0;
	unsigned int uiReadSize = 0;
   	unsigned int uiLastFrame = 0;
	int i = 0;
	int iRet = 0;

	int data[4] = {0x00,};
	unsigned char key[16] = {0x00,};

	/*** Read the key from OTP ********************************************/
	memset(&data,0x00,sizeof(int)*4);
	iRet = read_update_aes_key(data);
	if (iRet != 0) {
		printf("[Error] Read OTP fail : (%d)\n", iRet);
		return -1;
	}

	if (data[0] == 0x00) {
		printf("[Error] Read OTP is not seting )\n");
		return -2;
	}

	memset(&key,0x00,sizeof(unsigned char)*16);

	for (i = 0; i < 4; i++) {
		int index = i*4;
		key[index] = (unsigned char) ((data[i] & 0xff000000) >> 24);
		key[index+1] = (unsigned char) ((data[i] & 0x00ff0000) >> 16);
		key[index+2] = (unsigned char) ((data[i] & 0x0000ff00) >> 8);
		key[index+3] = (unsigned char) (data[i] & 0x000000ff);

//		printf("KEY[%d] : 0x%x \r\n",i,data[i]);
	}

	/*
	printf("[DEBUG key : ");
	for (i=0; i < 16; i++) {
		printf("0x%2x ,", key[i]);
	}
	printf("\r\n");
	*/
	/**********************************************************************/

	hSrcFile = fopen(pSrcFileName, "r");
	if(hSrcFile == NULL) {
		printf("Error fopen(%s)\n", pSrcFileName);
		return -5;
	}
	
	fseek(hSrcFile, 0, SEEK_END);
	uiFileSize = ftell(hSrcFile);
	fseek(hSrcFile, 0, SEEK_SET);

    hDstFile = fopen(pDstFileName, "w+");

	if(hDstFile == NULL) {
		printf("Error fopen(%s)\n", pDstFileName);
		fclose(hSrcFile);
		return -6;
	}

//	iRet = setAES128(hCipher, AES_CBC_ucInitVector, 16, AES_CBC_128KEY, 16, ucEnc);
	iRet = setAES128(hCipher, AES_CBC_ucInitVector, 16, key, 16, ucEnc);
	if(iRet) {
		printf("Error setAES128\n");
		fclose(hSrcFile);
		fclose(hDstFile);
		return -7;
	}

	printf("calc AES128(%d, %d)\n", uiFileSize, ucEnc);
	for(i=0; i<uiFileSize; i+=uiReadSize)
	{
		uiReadSize = fread(pSrcBuf, 1, REC_AES_BUF_SIZE, hSrcFile);
		printf("fread(%d, %d)\n", uiReadSize, REC_AES_BUF_SIZE);
		if(uiReadSize > 0) 
        {
#if 1 //jason.ku 2018.06.07 :: applied telechips patch
            if(uiReadSize != REC_AES_BUF_SIZE) {
				printf("Warning last frame\n");
				uiLastFrame = 1;
			}
			else {
				uiLastFrame = 0;
			}
			iRet = calcAES128(hCipher, pSrcBuf, &uiReadSize, pDstBuf, uiLastFrame);
#else
			iRet = calcAES128(hCipher, pSrcBuf, uiReadSize, pDstBuf);
#endif
			if(iRet) 
            {
				printf("Error calcAES128(%d, %d)\n", i, ucEnc);
				break;
			}

#if 1 //jason.ku 2018.06.07 :: applied telechips patch
			iRet = fwrite(pDstBuf, 1, uiReadSize, hDstFile);
			if(iRet != uiReadSize) {
				printf("Error fwrite(%d, %d)\n", iRet, uiReadSize);
				break;
			}

            if(uiLastFrame == 1) {
                printf("Exit loop\n");
                break;
            }

#else
			if(ucEnc == 1) 
            {
				iRet = fwrite(pDstBuf, 1, uiReadSize, hDstFile);
				if(iRet != uiReadSize) {
					printf("Error fwrite(%d, %d)\n", iRet, uiReadSize);
					break;
				}
			}
			else 
            {
                if(uiReadSize == REC_AES_BUF_SIZE)
                {
                    iRet = fwrite(pDstBuf, 1, uiReadSize, hDstFile);
                    if(iRet != uiReadSize) {
                        printf("1 Error fwrite(%d, %d)\n", iRet, uiReadSize);
                        break;
                    }
                }
                else// if(uiReadSize < REC_AES_BUF_SIZE)
                {
                    iRet = fwrite(pDstBuf, 1, uiReadSize-REC_AES_BLOCK_SIZE, hDstFile);
                    if(iRet != uiReadSize-REC_AES_BLOCK_SIZE) {
                        printf("2 Error fwrite(%d, %d)\n", iRet, uiReadSize);
                        break;
                    }
                }
			}
#endif
		}
	}

	if(hSrcFile) {
		fclose(hSrcFile);
	}

	if(hDstFile) {
		fclose(hDstFile);
	}	
   
	return 0;
}


int check_tcc_AES128(char* enc_input, char* dec_output)
{
    int hCipher = 0;
    unsigned char *pSrcBuf;
    unsigned int uiSrcBufSize = 0;
    unsigned char *pDstBuf;
    unsigned int uiDstBufSize;
    int iRet = 0;

    hCipher = open(CIPHER_DEVICE, O_RDWR);
    if(hCipher < 0) {
        printf("Error open cipher driver\n");
        return -1;
    }

    uiSrcBufSize = REC_AES_BUF_SIZE;
    pSrcBuf = (unsigned char *)calloc(1, uiSrcBufSize);
    if(pSrcBuf == NULL) {
        printf("Error calloc pSrcBuf\n");
        close(hCipher);
        return -1;
    }

    uiDstBufSize = REC_AES_BUF_SIZE;
    pDstBuf = (unsigned char *)calloc(1, uiDstBufSize);
    if(pSrcBuf == NULL) {
        printf("Error calloc pDstBuf\n");
        free(pSrcBuf);
        close(hCipher);
        return -1;
    }
    printf("\nAES-128 Decrypt\n\n");
    iRet = runAES128(hCipher, enc_input, dec_output, pSrcBuf, pDstBuf, 0);
    if(iRet < 0) {
        printf("Error runAES128 Decrypt(%d)\n", iRet);
        free(pSrcBuf);
        free(pDstBuf);
        close(hCipher);
        return -1;
    }

    if(hCipher >= 0) {
        close(hCipher);
    }

    free(pDstBuf);
    free(pSrcBuf);

    DeleteFile(enc_input);

    return 0;
}

rec_up_info_file get_info_file_str(char *buf)
{
	char *info_Str = NULL;
    rec_up_info_file up_info_str;
    
	printf("==> rec_info_Str :%s \n", buf);

    memset(&up_info_str, 0x00, sizeof(rec_up_info_file));
    up_info_str.cnt = 0;

	info_Str = strtok(buf, "- :");
    if(info_Str == NULL) {
		return up_info_str;
	}
	strcpy(up_info_str.f_name_str, info_Str);
    up_info_str.cnt = 1;

	info_Str = strtok(NULL, "- :");
    if(info_Str == NULL) {
		return up_info_str;
	}
	strcpy(up_info_str.sha224_hash, info_Str);
    up_info_str.cnt = 2;

	printf("file : %s, hash : %s \n",up_info_str.f_name_str,up_info_str.sha224_hash);

	return up_info_str;
}

int rec_info_file_parsing(const char *info_path)
{
	int cnt = 0;
    char info_buf[STR_MAX_LENGTH] = {0,};
    FILE *rfp = NULL;

    rfp = fopen(info_path, "r");
    if(rfp == NULL)
    {
        printf("File Open Error : %s !!\n", info_path);
        fclose(rfp);
        return -1;
    }

	memset(&rec_str_parser,0x00,sizeof(rec_up_info_file)*REC_UP_FILE_LIST_MAX);
    
    while(!feof(rfp))
    {
		memset(info_buf,0x00,sizeof(char)*STR_MAX_LENGTH);
        fgets(info_buf, sizeof(info_buf), rfp);
        rec_str_parser[cnt] = get_info_file_str(info_buf);
		cnt++;
    }
    fclose(rfp);

    return 0;
}


static int calcPad_2(unsigned char *pData, unsigned int uiSize)
{
	unsigned int uiTotalSize = uiSize;
	unsigned int i;

	pData[uiTotalSize++] = 0x80;
	for (i = (uiTotalSize % SHA_BLOCK_SIZE); (i > 0) && (i < SHA_BLOCK_SIZE); i++)
		pData[uiTotalSize++] = 0;

	if ((uiSize % SHA_BLOCK_SIZE) > 55) {
		for (i = 0; i < SHA_BLOCK_SIZE; i++)
			pData[uiTotalSize++] = 0;
	}

	*(unsigned int *)&pData[uiTotalSize-4] = BYTEORDER_SWAP32(uiSize*8);

	return uiTotalSize;
}

static int calcSHA224(int hCipher, unsigned char *pSrcBuf, unsigned int uiSrcBufSize, unsigned char *pDstBuf)
{
	stCIPHER_ALGORITHM algo;
	stCIPHER_RUN decr;
	unsigned int uiTotalSize;

	uiTotalSize = calcPad_2(pSrcBuf, uiSrcBufSize);

	algo.uOperationMode = TCC_CIPHER_OPMODE_ECB;
	algo.uAlgorithm = TCC_CIPHER_ALGORITM_HASH;
	algo.uArgument1 = SHA_224;
	algo.uArgument2 = (uiTotalSize / SHA_BLOCK_SIZE) - 1;//round;
	if(ioctl(hCipher, TCC_CIPHER_IOCTL_SET_ALGORITHM, &algo)) {
		printf("TCC_CIPHER_IOCTL_SET_ALGORITHM error\n");
		return -1;
	}

	decr.pucSrcAddr = pSrcBuf;
	decr.pucDstAddr = pDstBuf;
	decr.uLength = uiTotalSize;
	if(ioctl(hCipher, TCC_CIPHER_IOCTL_RUN, &decr)) {
		printf("TCC_CIPHER_IOCTL_RUN error\n");
		return -1;
	}

//	printf("HASH:\n");
//	printData(pDstBuf, SHA_DIGEST_SIZE);

	return 0;
}

int Run_SHA224_hash(const char * in_file, char *sha224_hash)
{
	int hCipher;
	FILE *hFile;
	int iFileSize;
	unsigned char *pSrcBuf;
	unsigned int uiSrcBufSize;
	unsigned char *pDstBuf;
	unsigned int uiDstBufSize;
	unsigned char *pDigestBuf;
	unsigned int uiDigestBufSize;
	int i;
	int iRet;
	
	hCipher = open(CIPHER_DEVICE, O_RDWR);
	if(hCipher < 0) {
		printf("Error open cipher driver\n");
		return -1;
	}

	hFile = fopen(in_file, "r");
	if(hFile == NULL) {
		printf("Error fopen test file\n");
		close(hCipher);
		return -2;
	}
	
	fseek(hFile, 0, SEEK_END);
	iFileSize = ftell(hFile);
	fseek(hFile, 0, SEEK_SET);

	uiSrcBufSize = SHA_MAX_BUF_SIZE;
	pSrcBuf = (unsigned char *)calloc(1, uiSrcBufSize);
	if(pSrcBuf == NULL) {
		printf("Error calloc pSrcBuf\n");
		fclose(hFile);
		close(hCipher);
		return -2;
	}

	uiDstBufSize = SHA_MAX_BUF_SIZE;
	pDstBuf = (unsigned char *)calloc(1, uiDstBufSize);
	if(pSrcBuf == NULL) {
		printf("Error calloc pDstBuf\n");
		free(pSrcBuf);
		fclose(hFile);
		close(hCipher);
		return -1;
	}

	uiDigestBufSize = (((iFileSize / SHA_BUF_SIZE) + 1) * SHA_DIGEST_SIZE) + (SHA_BLOCK_SIZE * 2);
	pDigestBuf = (unsigned char *)calloc(1, uiDigestBufSize);
	if(pDigestBuf == NULL) {
		printf("Error calloc pDigestBuf\n");
		free(pSrcBuf);
		free(pDstBuf);
		fclose(hFile);
		close(hCipher);
		return -1;
	}

	for(i=0,uiDigestBufSize=0; i<iFileSize; i+=uiSrcBufSize)
	{
		uiSrcBufSize = fread(pSrcBuf, 1, SHA_BUF_SIZE, hFile);
		if(uiSrcBufSize > 0) {
			if(uiSrcBufSize != SHA_BUF_SIZE) {
				printf("Warning last frame\n");
			}

			iRet = calcSHA224(hCipher, pSrcBuf, uiSrcBufSize, pDstBuf);
			if(iRet) {
				printf("Error 1st calcSHA224(%d)\n", i);
				break;
			}

			memcpy(&pDigestBuf[uiDigestBufSize], pDstBuf, SHA_DIGEST_SIZE);
			uiDigestBufSize += SHA_DIGEST_SIZE;
		}
		else if (uiSrcBufSize == 0) {
			printf("Read size is 0 \n");
			break;
		}
	}

	iRet = calcSHA224(hCipher, pDigestBuf, uiDigestBufSize, pDstBuf);
	if(iRet) {
		printf("Error 2nd calcSHA224\n");
	}
    
    for (i = 0; i < SHA224_DIGEST_SIZE; i++) 
    { 
		sprintf(&sha224_hash[i * 2], "%02x", pDstBuf[i]);
    }

	free(pDigestBuf);
	free(pDstBuf);
	free(pSrcBuf);

	if(hFile) {
		fclose(hFile);
	}

	if(hCipher >= 0) {
		close(hCipher);
	}

	return 0;
}

int check_validity_sha224(const char *file_path, const char *file_name)
{
    int loop = 0;
    int ret = 0;
	int file_name_length = 0;
	int sha_length = 0;
	char sha224_h[58] = {0,};

	printf("[DEBUG] %s, %d, file_path (%s),file_name (%s) \n",__func__,__LINE__,file_path,file_name);

	memset(&sha224_h,0x00,sizeof(char)*58);
    ret = Run_SHA224_hash(file_path, &sha224_h);
    if(ret == -1) {
		printf("Error open cipher driver\n");
        return -1;
    }
    else if(ret == -2) {
		printf("Memory allocation error\n");
        return -2;
    }
    else
    {
		file_name_length = strlen(file_name);
		sha_length = strlen(sha224_h);

        for(loop = 0; loop < REC_UP_FILE_LIST_MAX; loop++) {

            if(strncmp(rec_str_parser[loop].f_name_str, file_name, file_name_length) == 0) {

                if(strncmp(rec_str_parser[loop].sha224_hash, sha224_h, sha_length) == 0) {
					return 0;
                } else {
                	printf("sha224 Hash check fail !! \n");
                	printf("[info] : %s \n",rec_str_parser[loop].sha224_hash);
                	printf("[file] : %s \n",sha224_h);
					return -3;
				}
            }
        }
    }
	return -4;
}

#endif

/*** OTP API ***************************************************************************/
static int g_hOTP = 0;

int tcc_otp_open(const char * path)
{
	g_hOTP = open((const char*)path, O_RDWR);
	if(g_hOTP < 0) {
		printf("Error open otp driver(%d)\n", g_hOTP);
		return -1;
	}

	return 0;
}

int tcc_otp_close(void)
{
	int ret;

	ret = close(g_hOTP);
	if(ret < 0) {
		printf("Error close otp driver(%d)\n", ret);
		return -1;
	}

	return 0;
}

int tcc_otp_read(struct tcc_otp_ioctl_param *pstParam)
{
	int ret;

	ret = ioctl(g_hOTP, TCC_OTP_IOCTL_READ, pstParam);
	if(ret != 0) {
		printf("Error TCC_OTP_IOCTL_READ(%d)\n", ret);
		return -1;
	}

	return 0;
}

int tcc_otp_write(struct tcc_otp_ioctl_param *pstParam)
{
	int ret;

	ret = ioctl(g_hOTP, TCC_OTP_IOCTL_WRITE, pstParam);
	if(ret != 0) {
		printf("Error TCC_OTP_IOCTL_WRITE(%d)\n", ret);
		return -1;
	}

	return 0;
}

int read_public_key_otp(unsigned char *data, int size)
{
	int ret = 0;
	int loop = 0;
	int mbuf[128] = {0x00,};
	struct tcc_otp_ioctl_param stParam;

	ret = tcc_otp_open(TCC_OTP_DEIVCE_NAME);
	if(ret < 0) {
		printf("Error tcc_otp_open(%d)\n", ret);
		return -1;
	}

	stParam.addr = (unsigned int *)0x40001020;
	stParam.buf = mbuf;
	stParam.size = size;

	ret = tcc_otp_read(&stParam);
	if(ret < 0) {
		printf("Error tcc_otp_read(%d)\n", ret);
		return -2;
	}

	ret = tcc_otp_close();
	if(ret < 0) {
		printf("Error tcc_otp_close(%d)\n", ret);
		return -3;
	}

	memset(data,0x00,sizeof(unsigned char)*512);

	for (loop = 0;loop < 128; loop++) {
		int index = loop*4;
		data[index+3] =(unsigned char) ((mbuf[loop] & 0xFF000000) >> 24); 
		data[index+2] =(unsigned char) ((mbuf[loop] & 0x00FF0000) >> 16); 
		data[index+1] =(unsigned char) ((mbuf[loop] & 0x0000FF00) >>  8); 
		data[index]   =(unsigned char) ((mbuf[loop] & 0x000000FF)); 
//		printf("0x%x, 0x%x, 0x%x, 0x%x, ",data[index], data[index+1], data[index+2], data[index+3]);
	}

	return 0;
}

int write_public_key_otp(int *data, int size)
{
	int fd = 0;
	int mbuf[128] = {0x00,};

	int ret = 0;
	struct tcc_otp_ioctl_param stParam;

	fd = open("/upgrade/public_key.pem",O_RDONLY);
	if (fd < 0) {
		printf("[ERROR] Can't open file public_key.pem, %d\r\n",fd);
		return -1;
	}

	ret = read(fd,mbuf,512);
	if (ret < 0) {
		printf("[ERROR] read fail public_key.pem, %d\r\n",ret);
		close(fd);
		return -2;
	}

	close(fd);

	//////////////////////////////////////////////////////////////////

	ret = tcc_otp_open(TCC_OTP_DEIVCE_NAME);
	if(ret < 0) {
		printf("Error tcc_otp_open(%d)\n", ret);
		return -3;
	}

	stParam.addr = (unsigned int *)0x40001020;
	stParam.buf = mbuf;
	stParam.size = 0x1D0;

	ret = tcc_otp_write(&stParam);
	if(ret < 0) {
		printf("Error tcc_otp_read(%d)\n", ret);
		return -4;
	}

	ret = tcc_otp_close();
	if(ret < 0) {
		printf("Error tcc_otp_close(%d)\n", ret);
		return -5;
	}

	return 0;
}

int read_update_aes_key(int *data)
{
	int ret = 0;
	int loop = 0;
	struct tcc_otp_ioctl_param stParam;

	ret = tcc_otp_open(TCC_OTP_DEIVCE_NAME);
	if(ret < 0) {
		printf("Error tcc_otp_open(%d)\n", ret);
		return -1;
	}

	stParam.addr = (unsigned int *)0x40001000;
	stParam.buf = data;
	stParam.size = REC_AES_BLOCK_SIZE;

	ret = tcc_otp_read(&stParam);
	if(ret < 0) {
		printf("Error tcc_otp_read(%d)\n", ret);
		return -2;
	}

	ret = tcc_otp_close();
	if(ret < 0) {
		printf("Error tcc_otp_close(%d)\n", ret);
		return -3;
	}

	return 0;
}

int write_update_aes_key(void)
{
	int ret = 0;
	int loop = 0;
	unsigned int buf[4];
	struct tcc_otp_ioctl_param stParam;

	ret = tcc_otp_open(TCC_OTP_DEIVCE_NAME);
	if(ret < 0) {
		printf("Error tcc_otp_open(%d)\n", ret);
		return -1;
	}

	stParam.addr = (unsigned int *)0x40001000;
	stParam.buf = buf;
	stParam.buf[0] = 0x2b7e1516;
	stParam.buf[1] = 0x28aed2a6;
	stParam.buf[2] = 0xabf71588;
	stParam.buf[3] = 0x09cf4f3c;
	stParam.size = REC_AES_BLOCK_SIZE;

	ret = tcc_otp_write(&stParam);
	if(ret < 0) {
		printf("Error tcc_otp_read(%d)\n", ret);
		return -2;
	}

	ret = tcc_otp_close();
	if(ret < 0) {
		printf("Error tcc_otp_close(%d)\n", ret);
		return -3;
	}

	return 0;
}

int set_security_key_to_otp(void)
{
	int mBuf[4] = {0x00,};
	unsigned char optBuf[512] = {0x00,};
	int ret = 0;
	int loop = 0;

	if (access("/upgrade/public_key.pem",F_OK) == 0) {

		// Read public key
		ret = read_public_key_otp(optBuf,512);
		for (loop = 0; loop < 512; loop++) {
			if ((loop % 16) == 0) {
				printf("\r\n");
			}
			printf("0x%02x ",optBuf[loop]);
		}
		printf("\r\n");

		// Write public key
		ret = write_public_key_otp(NULL,0x1D0);

		ret = read_public_key_otp(optBuf,512);
		for (loop = 0; loop < 512; loop++) {
			if ((loop % 16) == 0) {
				printf("\r\n");
			}
			printf("0x%02x ",optBuf[loop]);
		}
		printf("\r\n");

		// Read AES128
		ret = read_update_aes_key(mBuf);
		for (loop = 0; loop < 4; loop++) {
			printf("0x%04x ",mBuf[loop]);
		}
		printf("\r\n");

		// Write AES128
		ret = write_update_aes_key();

		ret = read_update_aes_key(mBuf);
		for (loop = 0; loop < 4; loop++) {
			printf("0x%04x ",mBuf[loop]);
		}
		printf("\r\n");
	}

	return 0;
}

