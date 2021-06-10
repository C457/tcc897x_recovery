#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <assert.h>

typedef struct GnuTarHeader
{
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	unsigned char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
	char pad[12];
} GnuTarHeader;

void validateTarHeader(GnuTarHeader *tarHeader);
void parseFileSize(GnuTarHeader *tarHeader);
void parseFileName(GnuTarHeader *tarHeader);
void parseLongLink(GnuTarHeader *tarHeader, int fd);
void parseTarHeader(GnuTarHeader *tarHeader);

char currentFileName[512];

unsigned long long currentFileSize;
int lastLongLinkHeader;

char TAR_MAGIC[] = "ustar ";

int untar_function(char *tar_path, char *extract_file_name, char *dest_path)
{
    int fd;
    int tfd;
    int ret;
    unsigned long long seek;
    GnuTarHeader gnuHeader, emptyHeader;
    int emptyHeaders = 0;

    char *target_file;
    char *get_file_name = NULL;
    char file_name_buf[128] = {0};
    char curr_readFileName[512] = {0,};
    char fileName_tmp[512] = {0,};
    char temp_path[128] = {0,};
    int path_str_len = 0;
    char *file_name = NULL;
    unsigned char *buff;
  
	target_file = extract_file_name;

    fprintf(stderr, "=== jason : tar_path = %s, extract_file_name = %s\n", tar_path, extract_file_name);
    fprintf(stderr, "=== jason : dest_path = %s\n", dest_path);

    fd = open(tar_path, O_RDONLY);
    assert (fd != -1);

    memset (&emptyHeader, 0, 512);

    while (1) 
    {
        ret = read(fd, &gnuHeader, 512);
        assert(ret == 512);
        if (0 == memcmp(&gnuHeader, &emptyHeader, 512))
        {
            emptyHeaders ++;
            if (2 == emptyHeaders) {
                break;
            }
            continue;
        }
        emptyHeaders = 0;
        validateTarHeader(&gnuHeader);
        if ('L' == gnuHeader.typeflag) 
        {
            parseLongLink(&gnuHeader, fd);
        }
        else
        {
            parseTarHeader(&gnuHeader);
            fprintf(stderr, "=== jason : target_file = %s\n", target_file);
            fprintf(stderr, "=== jason : currentFileName = %s\n", currentFileName);

            fprintf(stderr, "\n==== compare file name ====\n\n");
            /* Individual decompression. */
            if(!strcmp(currentFileName, target_file))
            {
				umask(0);
                get_file_name = strrchr(target_file, '/');
                if(get_file_name != NULL)
                {
                    *get_file_name++;
                    fprintf(stderr, "=== jason : get_file_name = %s\n", get_file_name);
                }
                if(dest_path == NULL)
                {
                    sprintf(file_name_buf,"./%s",get_file_name);
                    fprintf(stderr, "=== jason 1 : file_name_buf = %s\n", file_name_buf);
                }
                else
                {
                    if(dest_path != NULL)
                    {
                        if(access(dest_path,F_OK) != 0)
                        {
                            mkdir(dest_path, 0755);
                            fprintf(stderr, "=====> jason : mkdir %s \r\n", dest_path);
                        }
                    }
                    sprintf(file_name_buf,"%s/%s",dest_path, get_file_name);
                    fprintf(stderr, "=== jason 2 : file_name_buf = %s\n", file_name_buf);
                }
                fprintf(stderr, "=== before open file_name_buf = %s\n", file_name_buf);
                tfd = open(file_name_buf, O_CREAT | O_RDWR, 0644);
                if(tfd < 0) 
                {
                    fprintf(stderr, "tfd open error [%d]\n", tfd);
                    perror("tfd open : ");
                    return -1;
                }

                buff = (unsigned char *)malloc(currentFileSize);
                ret = read(fd, buff, currentFileSize);
                if (ret < 0)
                {
                    fprintf(stderr, "read failed with return value: %ld\n",ret);
                    return -1;
                }
                ret = write(tfd, buff, currentFileSize);
                if (ret < 0)
                {
                    fprintf(stderr, "write failed with return value: %ld\n",ret);
                    return -1;
                }
                break;
            }
            else
            {
                fprintf(stderr, "tfd open error [%d]\n", tfd);
                seek = (currentFileSize/512) + (currentFileSize%512 ? 1 : 0);
                seek *= 512;
                seek = lseek(fd, seek, SEEK_CUR);
                assert(seek != -1);
            }
        }
    }
    return 0;
}



void validateTarHeader(GnuTarHeader *tarHeader)
{
    int i = 0;
	for (i=0; i<6; i++)
    {
		assert(tarHeader->magic[i] == TAR_MAGIC[i]);
	}
}

void parseFileSize(GnuTarHeader *tarHeader)
{
	int i;

	// parse the file size.
	currentFileSize = 0;

	if (tarHeader->size[0] & (0X01 << 7)) {
		// file size > 8 GB.
		for (i=1; i<12; i++) {
			currentFileSize *= 256;
			currentFileSize += tarHeader->size[i];
		}
	} else {
		// file size < 8 GB.
		for (i=0; i<12; i++) {
			if ((0 == tarHeader->size[i]) || (' ' == tarHeader->size[i])) {
				continue;
			}
			currentFileSize *= 8;
			currentFileSize += (tarHeader->size[i] - '0');
		}
	}
}

void parseFileName(GnuTarHeader *tarHeader)
{
	int i;
	char fileName[256];

	memset(currentFileName, 0, 512);

	fprintf(stderr,"===> %s\n", __func__);
	if (0 != tarHeader->prefix[0]) {
		for (i=0; i<155; i++) {
			if (0 == tarHeader->prefix[i]) {
				break;
			}
			fileName[i] = tarHeader->prefix[i];
      
		}
		fileName[i] = '\0';
		strcpy(currentFileName, fileName);
		currentFileName[strlen(fileName)] = '/';
		currentFileName[strlen(fileName) + 1] = '/';
	}

	for (i=0; i<100; i++) {
		if (0 == tarHeader->name[0]) {
			break;
		}
		fileName[i] = tarHeader->name[i];
    //fprintf(stderr,"==> jason :fileName[%d] = %s\n", i,fileName[i]);
	}

	fileName[i] = '\0';
	strcpy(currentFileName + strlen(currentFileName), fileName);
}

void parseLongLink(GnuTarHeader *tarHeader, int fd)
{
	int ret;
	char fileName[512+1]; // last byte for '\0''

	memset(currentFileName, 0, 512);
	parseFileSize(tarHeader);
	while(1) {
		ret = read (fd, fileName, 512);
		if (currentFileSize > 512) {
			fileName[512] = '\0';
		} else {
			fileName[currentFileSize] = '\0';
			strcpy(currentFileName, fileName);
			break;
		}
		currentFileSize -= 512;
		strcpy(currentFileName + strlen(currentFileName), fileName);
	}

	lastLongLinkHeader = 1;
}

void parseTarHeader(GnuTarHeader *tarHeader)
{
	parseFileSize(tarHeader);

	// parse the filename.
	if (0 == lastLongLinkHeader) {
		parseFileName(tarHeader);
	}

	lastLongLinkHeader = 0;
	fprintf(stderr,"%s\n", __func__);
	fprintf(stderr,"==> jason : %s : size = %llu\n", currentFileName, currentFileSize);
}

int isdir(char path[])
{
	struct stat dir;
	stat(path,&dir);
	if(S_ISREG(dir.st_mode))//checks if string is file
		return 0;
	if(S_ISDIR(dir.st_mode))//checks if string is directory
		return 1;
	fprintf(stderr,"###  The source is not a file, neither a dir\n");
	return -1;
}


long DeleteFile(const char*	strPath)
{
	int ret = 0;

	fprintf(stderr,"%s: %s\n", __func__, strPath);
	ret = unlink(strPath);
	fprintf(stderr," unlink value: %d, errno: %d\n", ret, ret ? errno : 0);

	if (ret < 0 && errno != ENOENT)	//if file does not exist then we can say that we deleted it successfully
	{
		fprintf(stderr, "delete failed with return value: %d errno %d\n",ret, errno);
		return -1;
	}
	return 0;
}

long DeleteFolder(const char*	strPath)
{
	int ret = 0;

	fprintf(stderr,"%s: %s\n", __func__, strPath);
    ret = rmdir(strPath);
	fprintf(stderr," rmdir value: %d, errno: %d\n", ret, errno);

    if ((ret == 0) || ((ret < 0) && ((errno == ENOENT) || (errno == ENOTEMPTY ))))
    	return 0;

    fprintf(stderr,"Delete folder problem value: %d, errno: %d\n", ret, errno);
 	return -1;
}

