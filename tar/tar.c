#include "tar.h"


int rc = -1;

int tar_read(const int fd, struct tar_t ** archive, const char verbosity)
{
    if (fd < 0){
        return -1;
    }

    if (!archive || *archive){
        return -1;
    }

    unsigned int offset = 0;
    int count = 0;

    struct tar_t ** tar = archive;
    char update = 1;
    for(count = 0; ; count++){
        *tar = malloc(sizeof(struct tar_t));
        if (update && (read_size(fd, (*tar) -> block, 512) != 512)){
        	fprintf(stderr, "[2017.05.26] Error: Bad read. Stopping\n");
            tar_free(*tar);
            *tar = NULL;
            break;
        }

        update = 1;
        // if current block is all zeros
        if (iszeroed((*tar) -> block, 512)){
            if (read_size(fd, (*tar) -> block, 512) != 512){
            	fprintf(stderr, "[2017.05.26] Error: Bad read. Stopping\n");
                tar_free(*tar);
                *tar = NULL;
                break;
            }

            // check if next block is all zeros as well
            if (iszeroed((*tar) -> block, 512)){
                tar_free(*tar);
                *tar = NULL;

                // skip to end of record
                if (lseek(fd, RECORDSIZE - (offset % RECORDSIZE), SEEK_CUR) == (off_t) (-1)){
                	fprintf(stderr, "[2017.05.26] Error: Unable to seek file: %s\n", strerror(rc));
                }

                break;
            }

            update = 0;
        }

        // set current entry's file offset
        (*tar) -> begin = offset;

        // skip over data and unfilled block
        unsigned int jump = oct2uint((*tar) -> size, 11);
        if (jump % 512){
            jump += 512 - (jump % 512);
        }

        // move file descriptor
        offset += 512 + jump;
        if (lseek(fd, jump, SEEK_CUR) == (off_t) (-1)){
        	fprintf(stderr, "[2017.05.26] Error: Unable to seek file: %s\n", strerror(rc));
        }

        // ready next value
        tar = &((*tar) -> next);
    }

    return count;
}

int tar_write(const int fd, struct tar_t ** archive, const size_t filecount, const char * files[], const char verbosity)
{
    if (fd < 0){
        return -1;
    }

    if (!archive){
        return -1;
    }

    // where file descriptor offset is
    int offset = 0;

    // if there is old data
    struct tar_t ** tar = archive;
    if (*tar){
        // skip to last entry
        while (*tar && (*tar) -> next){
            tar = &((*tar) -> next);
        }

        // get offset past final entry
        unsigned int jump = 512 + oct2uint((*tar) -> size, 11);
        if (jump % 512){
            jump += 512 - (jump % 512);
        }

        // move file descriptor
        offset = (*tar) -> begin + jump;
        if (lseek(fd, offset, SEEK_SET) == (off_t) (-1)){
        	fprintf(stderr, "[2017.05.26] Error: Unable to seek file: %s\n", strerror(rc));
        }
        tar = &((*tar) -> next);
    }

    // write entries first
    if (write_entries(fd, tar, archive, filecount, files, &offset, verbosity) < 0){
    	fprintf(stderr, "[2017.05.26] Error: Failed to write entries\n");
        return -1;
    }

    // write ending data
    if (write_end_data(fd, offset, verbosity) < 0){
    	fprintf(stderr, "[2017.05.26] Error: Failed to write end data");
        return -1;
    }

    // clear original names from data
    tar = archive;
    while (*tar){
        memset((*tar) -> name, 0, 100);
        tar = &((*tar) -> next);
    }
    return offset;
}

void tar_free(struct tar_t * archive)
{
    while (archive){
        struct tar_t * next = archive -> next;
        free(archive);
        archive = next;
    }
}

int tar_ls(FILE * f, struct tar_t * archive, const size_t filecount, const char * files[], const char verbosity)
{
    if (!verbosity){
        return 0;
    }

    if (filecount && !files){
        return -1;
    }

    unsigned int max_space = 0;
    while (archive){
        if (ls_entry(f, archive, &max_space, filecount, files, verbosity) < 0){
            return -1;
        }
        archive = archive -> next;
    }

    return 0;
}

//int tar_extract(const int fd, struct tar_t * archive, const size_t filecount, const char * files[], const char verbosity){
int tar_extract(const int fd, struct tar_t * archive, const size_t filecount, const char * files[], char * output, const char verbosity)
{
    int ret = 0;

    // extract entries with given names
    if (filecount){
        if (!files){
        	fprintf(stderr, "[2017.05.26] Error: Received non-zero file count but got NULL file list\n");
            return -1;
        }

        while (archive){
			size_t i;
            for(i = 0; i < filecount; i++){
                if (!strncmp(archive -> name, files[i], MAX(strlen(archive -> name), strlen(files[i])))){
                    if (lseek(fd, archive -> begin, SEEK_SET) == (off_t) (-1)){
                    	fprintf(stderr, "[2017.05.26] Error: Unable to seek file: %s\n", strerror(rc));
                    }

                    if (extract_entry(fd, archive, output, verbosity) < 0){
                        ret = -1;
                    }
                    break;
                }
            }
            archive = archive -> next;
        }
    }
    // extract all
    else{
        // move offset to beginning
        if (lseek(fd, 0, SEEK_SET) == (off_t) (-1)){
        	fprintf(stderr, "[2017.05.26] Error: Unable to seek file: %s\n", strerror(rc));
        }

        // extract each entry
        while (archive){
        	if (extract_entry(fd, archive, output, verbosity) < 0){
                ret = -1;
            }
            archive = archive -> next;
        }
    }

    return ret;
}

int tar_update(const int fd, struct tar_t ** archive, const size_t filecount, const char * files[], const char verbosity)
{
    if (!filecount){
        return 0;
    }

    if (filecount && !files){
    	fprintf(stderr, "[2017.05.26] Error: Non-zero file count provided, but file list is NULL");
        return -1;
    }

    // buffer for subset of files that need to be updated
    char ** newer = calloc(filecount, sizeof(char *));

    struct stat st;
    int count = 0;
    int all = 1;

    // check each source to see if it was updated
    struct tar_t * tar = *archive;
	int i;
    for(i = 0; i < filecount; i++){
        // make sure original file exists
        if (lstat(files[i], &st)){
            all = 0;
            fprintf(stderr, "[2017.05.26] Error: Could not stat %s: %s\n", files[i], strerror(rc));
        }

        // find the file in the archive
        struct tar_t * old = exists(tar, files[i], 1);
        newer[count] = calloc(strlen(files[i]) + 1, sizeof(char));

        // if there is an older version, check its timestamp
        if (old){
            if (st.st_mtime > oct2uint(old -> mtime, 11)){
                strncpy(newer[count++], files[i], strlen(files[i]));
                fprintf(stderr, "[2017.05.26] %s\n", files[i]);
            }
        }
        // if there is no older version, just add it
        else{
            strncpy(newer[count++], files[i], strlen(files[i]));
            fprintf(stderr, "[2017.05.26] %s\n", files[i]);
        }
    }

    // update listed files only
    if (tar_write(fd, archive, count, (const char **) newer, verbosity) < 0){
    	fprintf(stderr, "[2017.05.26] Error: Unable to update archive\n");
        return -1;
    }

    // cleanup
    for(i = 0; i < count; i++){
        free(newer[i]);
    }
    free(newer);

    return all?0:-1;
}

int tar_remove(const int fd, struct tar_t ** archive, const size_t filecount, const char * files[], const char verbosity)
{
    if (fd < 0){
        return -1;
    }

    // archive has to exist
    if (!archive || !*archive){
        return -1;
    }

    if (filecount && !files){
        return -1;
    }

    if (!filecount){
        return 0;
    }

    // get file permissions
    struct stat st;
    if (fstat(fd, &st)){
    	fprintf(stderr, "[2017.05.26] Error: Unable to stat archive: %s\n", strerror(rc));
    }

    // reset offset of original file
    if (lseek(fd, 0, SEEK_SET) == (off_t) (-1)){
    	fprintf(stderr, "[2017.05.26] Error: Unable to seek file: %s\n", strerror(rc));
    }

    // find first file to be removed that does not exist
    int ret = 0;
    char * bad = calloc(filecount, sizeof(char));
	int i;
    for(i = 0; i < filecount; i++){
        if (!exists(*archive, files[i], 0)){
        	fprintf(stderr, "[2017.05.26] Error: %s not found in archive\n", files[i]);
            bad[i] = 1;
            ret = -1;
        }
    }

    unsigned int read_offset = 0;
    unsigned int write_offset = 0;
    struct tar_t * prev = NULL;
    struct tar_t * curr = *archive;
    while(curr){
        // get original size
        int total = 512;

        if ((curr -> type == REGULAR) || (curr -> type == NORMAL) || (curr -> type == CONTIGUOUS)){
            total += oct2uint(curr -> size, 11);
            if (total % 512){
                total += 512 - (total % 512);
            }
        }
        const int match = check_match(curr, filecount, bad, files);

        if (match < 0){
        	fprintf(stderr, "[2017.05.26] Error: Match failed\n");
            return -1;
        }
        else if (!match){
            // if the old data is not in the right place, move it
            if (write_offset < read_offset){
                int got = 0;
                while (got < total){
                    // go to old data
                    if (lseek(fd, read_offset, SEEK_SET) == (off_t) (-1)){
                    	fprintf(stderr, "[2017.05.26] Error: Cannot seek: %s\n", strerror(rc));
                    }

                    char buf[512];

                    // copy chunk out
                    if (read_size(fd, buf, 512) != 512){// guarenteed 512 octets
                    	fprintf(stderr, "[2017.05.26] Error: Read error\n");
                        return -1;
                    }

                    // go to new position
                    if (lseek(fd, write_offset, SEEK_SET) == (off_t) (-1)){
                    	fprintf(stderr, "[2017.05.26] Error: Cannot seek: %s\n", strerror(rc));
                    }

                    // write data in
                    if (write_size(fd, buf, 512) != 512){
                    	fprintf(stderr, "[2017.05.26] Error: Write error\n");
                        return -1;
                    }

                    // increment offsets
                    got += 512;
                    read_offset += 512;
                    write_offset += 512;
                }
            }
            else{
                read_offset += total;
                write_offset += total;

                // skip past data
                if (lseek(fd, read_offset, SEEK_SET) == (off_t) (-1)){
                	fprintf(stderr, "[2017.05.26] Error: Cannot seek: %s\n", strerror(rc));
                }
            }
            prev = curr;
            curr = curr -> next;
        }
        else{// if name matches, skip the data
            struct tar_t * tmp = curr;
            if (!prev){
                *archive = curr -> next;
                if (*archive){
                    (*archive) -> begin = 0;
                }
            }
            else{
                prev -> next = curr -> next;

                if (prev -> next){
                    prev -> next -> begin = curr -> begin;
                }
            }
            curr = curr -> next;
            free(tmp);

            // next read starts after current entry
            read_offset += total;
        }
    }

    // resize file
    if (ftruncate(fd, write_offset) < 0){
    	fprintf(stderr, "[2017.05.26] Error: Could not truncate file: %s\n", strerror(rc));
    }

    // add end data
    if (write_end_data(fd, write_offset, verbosity) < 0){
    	fprintf(stderr, "[2017.05.26] Error: Could not close file\n");
    }

    return ret;
}

int tar_diff(FILE * f, struct tar_t * archive, const char verbosity)
{
    struct stat st;
    while (archive){
    	fprintf(stderr, "[2017.05.26] %s\n", archive -> name);

        // if not found, print error
        if (lstat(archive -> name, &st)){
            int rc = errno;
            fprintf(stderr, "[2017.05.26] Error: Could not ");
            if (archive -> type == SYMLINK){
                fprintf(stderr, "[2017.05.26] readlink");
            }
            else{
                fprintf(stderr, "[2017.05.26] stat");
            }
            fprintf(stderr,"[2017.05.26] %s: %s\n", archive -> name, strerror(rc));
        }
        else{
            if (st.st_mtime != oct2uint(archive -> mtime, 11)){
            	fprintf(stderr, "[2017.05.26] %s: Mod time differs", archive -> name);
            }
            if (st.st_size != oct2uint(archive -> size, 11)){
            	fprintf(stderr, "[2017.05.26] %s: Mod time differs", archive -> name);
            }
        }

        archive = archive -> next;
    }
    return 0;
}

int print_entry_metadata(FILE * f, struct tar_t * entry){
    if (!entry){
        return -1;
    }

    time_t time = oct2uint(entry -> mtime, 12);
    fprintf(stderr, "[2017.05.26] File Name: %s\n", entry -> name);
    fprintf(stderr, "[2017.05.26] File Mode: %03o\n", oct2uint(entry -> mode, 8));
    fprintf(stderr, "[2017.05.26] Owner UID: %d\n", oct2uint(entry -> uid, 12));
    fprintf(stderr, "[2017.05.26] Owner GID: %d\n", oct2uint(entry -> gid, 12));
    fprintf(stderr, "[2017.05.26] File Size: %d\n", oct2uint(entry -> size, 12));
    fprintf(stderr, "[2017.05.26] Time     : %s", asctime(gmtime(&time)));
    fprintf(stderr, "[2017.05.26] Checksum : %s\n", entry -> check);
    fprintf(stderr, "[2017.05.26] File Type: ");

    switch (entry -> type){
        case REGULAR: case NORMAL:
        	fprintf(stderr, "[2017.05.26] Normal File");
            break;
        case HARDLINK:
        	fprintf(stderr, "[2017.05.26] Hard Link");
            break;
        case SYMLINK:
        	fprintf(stderr, "[2017.05.26] Symbolic Link");
            break;
        case CHAR:
        	fprintf(stderr, "[2017.05.26] Character Special");
            break;
        case BLOCK:
        	fprintf(stderr, "[2017.05.26] Block Special");
            break;
        case DIRECTORY:
        	fprintf(stderr, "[2017.05.26] Directory");
            break;
        case FIFO:
        	fprintf(stderr, "[2017.05.26] FIFO");
            break;
        case CONTIGUOUS:
        	fprintf(stderr, "[2017.05.26] Contiguous File");
            break;
    }

    fprintf(stderr, "[2017.05.26]  (%c)\n", entry -> type?entry -> type:'0');
    fprintf(stderr, "[2017.05.26] Link Name: %s\n", entry -> link_name);
    fprintf(stderr, "[2017.05.26] Ustar\\000: %c%c%c%c%c\\%2x\\%2x\\%02x\n",   entry -> ustar[0], entry -> ustar[1], entry -> ustar[2], entry -> ustar[3], entry -> ustar[4], entry -> ustar[5], entry -> ustar[6], entry -> ustar[7]);
    fprintf(stderr, "[2017.05.26] Username : %s\n", entry -> owner);
    fprintf(stderr, "[2017.05.26] Group    : %s\n", entry -> group);
    fprintf(stderr, "[2017.05.26] Major    : %s\n", entry -> major);
    fprintf(stderr, "[2017.05.26] Minor    : %s\n", entry -> minor);
    fprintf(stderr, "[2017.05.26] Prefix   : %s\n", entry -> prefix);
    fprintf(stderr, "[2017.05.26] \n");

    return 0;
}

int print_tar_metadata(FILE * f, struct tar_t * archive){
    while (archive){
        print_entry_metadata(f, archive);
        archive = archive -> next;
    }
    return 0;
}

struct tar_t * exists(struct tar_t * archive, const char * filename, const char ori){
    while (archive){
        if (ori){
            if (!strncmp(archive -> original_name, filename, MAX(strlen(archive -> original_name), strlen(filename)) + 1)){
                return archive;
            }
        }
        else{
            if (!strncmp(archive -> name, filename, MAX(strlen(archive -> name), strlen(filename)) + 1)){
                return archive;
            }
        }
        archive = archive -> next;
    }
    return NULL;
}

int format_tar_data(struct tar_t * entry, const char * filename, const char verbosity){
    if (!entry){
        return -1;
    }

    struct stat st;
    if (lstat(filename, &st)){
        fprintf(stderr, "[2017.05.26] Error: Cannot stat %s: %s\n", filename, strerror(rc));
        return -1;
    }

    // remove relative path
    int move = 0;
    if (!strncmp(filename, "/", 1)){
        move = 1;
    }
    else if (!strncmp(filename, "./", 2)){
        move = 2;
    }
    else if (!strncmp(filename, "../", 3)){
        move = 3;
    }

    // start putting in new data
    memset(entry, 0, sizeof(struct tar_t));
    strncpy(entry -> original_name, filename, 100);
    strncpy(entry -> name, filename + move, 100);
    sprintf(entry -> mode, "%07o", st.st_mode & 0777);
    sprintf(entry -> uid, "%07o", st.st_uid);
    sprintf(entry -> gid, "%07o", st.st_gid);
    sprintf(entry -> size, "%011o", (int) st.st_size);
    sprintf(entry -> mtime, "%011o", (int) st.st_mtime);
    strncpy(entry -> group, "None", 4);                     // default value
    memcpy(entry -> ustar, "ustar\00000", 8);               // official value?

    // figure out filename type
    switch (st.st_mode & S_IFMT) {
        case S_IFREG:
            entry -> type = NORMAL;
            break;
        case S_IFLNK:
            entry -> type = SYMLINK;

            // file size is 0, but will print link size
            strncpy(entry -> size, "00000000000", 11);

            // get link name
            if (readlink(filename, entry -> link_name, 100) < 0){
                fprintf(stderr, "[2017.05.26] Error: Could not read link %s: %s\n", filename, strerror(rc));
            }

            break;
        case S_IFCHR:
            entry -> type = CHAR;
            // get character device major and minor values
            sprintf(entry -> major, "%08o", major(st.st_dev));
            sprintf(entry -> minor, "%08o", minor(st.st_dev));
            break;
        case S_IFBLK:
            entry -> type = BLOCK;
            // get block device major and minor values
            sprintf(entry -> major, "%08o", major(st.st_dev));
            sprintf(entry -> minor, "%08o", minor(st.st_dev));
            break;
        case S_IFDIR:
            entry -> type = DIRECTORY;
            break;
        case S_IFIFO:
            entry -> type = FIFO;
            break;
        case S_IFSOCK:
            entry -> type = -1;
            fprintf(stderr, "[2017.05.26] Error: Cannot tar socket\n");
            return -1;
            break;
        default:
            entry -> type = -1;
            fprintf(stderr, "[2017.05.26] Error: Unknown filetype\n");
            return -1;
            break;
    }

    // get username
    if (getlogin_r(entry -> owner, 32)){
        fprintf(stderr, "[2017.05.26] Warning: Unable to get username: %s\n", strerror(rc));
    }

    // get group name
    struct group * grp = getgrgid(st.st_gid);
    if (grp){
        //strncpy(entry -> group, grp -> gr_name, 100);
        strncpy(entry -> group, grp -> gr_name, sizeof(entry -> group));
    }

    // get the checksum
    calculate_checksum(entry);

    return 0;
}

unsigned int calculate_checksum(struct tar_t * entry){
    // use 8 spaces (' ', char 0x20) in place of checksum string
    memset(entry -> check, ' ', sizeof(char) * 8);

    // sum of entire metadata
    unsigned int check = 0;
	int i;
    for(i = 0; i < 500; i++){
        check += (unsigned char) entry -> block[i];
    }
    sprintf(entry -> check, "%06o", check);

    entry -> check[6] = '\0';
    entry -> check[7] = ' ';
    return check;
}

int ls_entry(FILE * f, struct tar_t * entry, unsigned int * max_space, const size_t filecount, const char * files[], const char verbosity){
    if (!verbosity){
        return 0;
    }

    if (filecount && !files){
        fprintf(stderr, "[2017.05.26] Error: Non-zero file count given but no files given\n");
        return -1;
    }

    // figure out whether or not to print
    char print = !filecount;
	size_t i;
    for(i = 0; i < filecount; i++){
        if (strncmp(entry -> name, files[i], MAX(strlen(entry -> name), strlen(files[i])))){
            print = 1;
            break;
        }
    }

    if (print){
        if (verbosity > 1){
            const mode_t mode = oct2uint(entry -> mode, 7);
            const char mode_str[26] = { "-hlcbdp-"[entry -> type?entry -> type - '0':0],
                                        mode & S_IRUSR?'r':'-',
                                        mode & S_IWUSR?'w':'-',
                                        mode & S_IXUSR?'x':'-',
                                        mode & S_IRGRP?'r':'-',
                                        mode & S_IWGRP?'w':'-',
                                        mode & S_IXGRP?'x':'-',
                                        mode & S_IROTH?'r':'-',
                                        mode & S_IWOTH?'w':'-',
                                        mode & S_IXOTH?'x':'-',
                                        0};
            fprintf(stderr, "[2017.05.26] %s %s/%s ", mode_str, entry -> owner, entry -> group);
            char size_buf[22] = {0};
            int rc = -1;
            switch (entry -> type){
                case REGULAR: case NORMAL: case CONTIGUOUS:
                    rc = sprintf(size_buf, "  %u", oct2uint(entry -> size, 11));
                    break;
                case HARDLINK: case SYMLINK: case DIRECTORY: case FIFO:
                    rc = sprintf(size_buf, "  %u", oct2uint(entry -> size, 11));
                    break;
                case CHAR: case BLOCK:
                    rc = sprintf(size_buf, "%d,%d", oct2uint(entry -> major, 7), oct2uint(entry -> minor, 7));
                    break;
            }

            if (rc < 0){
                fprintf(stderr, "[2017.05.26] Error: Failed to write length\n");
                return -1;
            }

            // update padding space
            if (*max_space < rc){
                *max_space = rc;
            }

            // print padding space
			unsigned int i;
            for(i = 0; i < (*max_space - rc); i++){
                fprintf(stderr, "[2017.05.26]  ");
            }

            fprintf(stderr, "[2017.05.26] %s", size_buf);

            time_t mtime = oct2uint(entry -> mtime, 11);
            struct tm * time = localtime(&mtime);
            fprintf(stderr, "[2017.05.26]  %d-%02d-%02d %02d:%02d ", time -> tm_year + 1900, time -> tm_mon + 1, time -> tm_mday, time -> tm_hour, time -> tm_min);
        }

        fprintf(stderr, "[2017.05.26] %s", entry -> name);

        if (verbosity > 1){
            switch (entry -> type){
                case HARDLINK:
                    fprintf(stderr, "[2017.05.26]  link to %s", entry -> link_name);
                    break;
                case SYMLINK:
                    fprintf(stderr, "[2017.05.26]  -> %s", entry -> link_name);
                    break;
                break;
            }
        }

        fprintf(stderr, "[2017.05.26] \n");
    }

    return 0;
}

//int extract_entry(const int fd, struct tar_t * entry, const char verbosity){
int extract_entry(const int fd, struct tar_t * entry, char *output, const char verbosity){
    fprintf(stderr,"[2017.05.26] extract_entry >> entry -> name : %s\n", entry -> name);

    if ((entry -> type == REGULAR) || (entry -> type == NORMAL) || (entry -> type == CONTIGUOUS)){
        // create intermediate directories
        size_t len = strlen(entry -> name);
        char * path = calloc(len + 1, sizeof(char));
        strncpy(path, entry -> name, len);

        char *destination_file = calloc(strlen(entry -> name) + strlen(output) + 1, sizeof(char));
        strncpy(destination_file, output, strlen(output));
        strcat(destination_file, "/");
        strcat(destination_file, entry -> name);

        fprintf(stderr,"[2017.05.26] extract_entry >> destination_file : %s\n", destination_file);

        // remove file from path
        while (--len && (path[len] != '/'));
        path[len] = '\0';   // if nothing was found, path is terminated
        fprintf(stderr,"[2017.05.26] extract_entry >> Path %s\n", path);

        struct stat sb;

		if (stat(path, &sb) != 0) {
			if (recursive_mkdir(path, oct2uint(entry -> mode, 7) & 0777, verbosity) < 0){
				fprintf(stderr, "[2017.05.26] Error[1]: Could not make directory %s: %s\n", path);
				free(path);
				return -1;
			}
		} else if (!S_ISDIR(sb.st_mode)) {
			/* not a directory */
			fprintf(stderr, "[2017.05.26] Error: %s is not a directory\n", path);
			free(path);
			return -1;
		}
		free(path);

        if ((entry -> type == REGULAR) || (entry -> type == NORMAL) || (entry -> type == CONTIGUOUS)){
            // create file
            const unsigned int size = oct2uint(entry -> size, 11);
            int f = open(destination_file, O_WRONLY | O_CREAT | O_TRUNC, oct2uint(entry -> mode, 7) & 0777);
            if (f < 0){
            	fprintf(stderr, "[2017.05.26] Error: Unable to open file %s: %s\n", entry -> name, strerror(rc));
            }

            // move archive pointer to data location
            if (lseek(fd, 512 + entry -> begin, SEEK_SET) == (off_t) (-1)){
                fprintf(stderr, "[2017.05.26] Error: Bad index: %s\n", strerror(rc));
            }

            // copy data to file
            char buf[512];
            int got = 0;
            while (got < size){
                int r;
                if ((r = read_size(fd, buf, MIN(size - got, 512))) < 0){
                    fprintf(stderr, "[2017.05.26] Error: Unable to read from archive: %s\n", strerror(rc));
                }

                if (write(f, buf, r) != r){
                    fprintf(stderr, "[2017.05.26] Error: Unable to write to %s: %s\n", entry -> name, strerror(rc));
                }

                got += r;
            }

            close(f);

            /////////////////////////////////////////
			chmod(destination_file, 0755);
			chown(destination_file, 1000, 1000);   // system user
			/////////////////////////////////////////
        }
        else if ((entry -> type == CHAR) || (entry -> type == BLOCK)){
            if (mknod(entry -> name, oct2uint(entry -> mode, 7), (oct2uint(entry -> major, 7) << 20) | oct2uint(entry -> minor, 7)) < 0){
                fprintf(stderr, "[2017.05.26] Error: Unable to make device %s: %s\n", entry -> name, strerror(rc));
            }
        }
    }
    else if (entry -> type == HARDLINK){
        if (link(entry -> link_name, entry -> name) < 0){
            fprintf(stderr, "[2017.05.26] Error: Unable to create hardlink %s: %s\n", entry -> name, strerror(rc));
        }
    }
    else if (entry -> type == SYMLINK){
        if (symlink(entry -> link_name, entry -> name) < 0){
            fprintf(stderr, "[2017.05.26] Error: Unable to make symlink %s: %s\n", entry -> name, strerror(rc));
        }
    }
    else if (entry -> type == CHAR){
        if (mknod(entry -> name, S_IFCHR | (oct2uint(entry -> mode, 7) & 0777), (oct2uint(entry -> major, 7) << 20) | oct2uint(entry -> minor, 7)) < 0){
            fprintf(stderr, "[2017.05.26] Error: Unable to create directory %s: %s\n", entry -> name, strerror(rc));
        }
    }
    else if (entry -> type == BLOCK){
        if (mknod(entry -> name, S_IFBLK | (oct2uint(entry -> mode, 7) & 0777), (oct2uint(entry -> major, 7) << 20) | oct2uint(entry -> minor, 7)) < 0){
            fprintf(stderr, "[2017.05.26] Error: Unable to create directory %s: %s\n", entry -> name, strerror(rc));
        }
    }
    else if (entry -> type == DIRECTORY){
    	 char *destination_dir = calloc(strlen(entry -> name) + strlen(output) + 1, sizeof(char));
		strncpy(destination_dir, output, strlen(output));
		strcat(destination_dir, "/");
		strcat(destination_dir, entry -> name);
		fprintf(stderr,"[2017.05.26] extract_entry >> destination_file : %s\n", destination_dir);

		struct stat sb;

		if (stat(destination_dir, &sb) != 0) {
			if (recursive_mkdir(destination_dir, oct2uint(entry -> mode, 7) & 0777, verbosity) < 0){
				fprintf(stderr, "[2017.05.26] Error: Unable to create directory %s: %s\n", entry -> name, strerror(rc));
			}
		} else if (!S_ISDIR(sb.st_mode)) {
            /* not a directory */
            fprintf(stderr, "[2017.05.26] Error: %s is not a directory\n", entry -> name);
            return -1;
        }

		/////////////////////////////////////////
		chmod(destination_dir, 0755);
		chown(destination_dir, 1000, 1000);   // system user
		/////////////////////////////////////////
    }
    else if (entry -> type == FIFO){
        if (mkfifo(entry -> name, oct2uint(entry -> mode, 7) & 0777) < 0){
            fprintf(stderr, "[2017.05.26] Error: Unable to make pipe %s: %s\n", entry -> name, strerror(rc));
        }
    }
    return 0;
}

int write_entries(const int fd, struct tar_t ** archive, struct tar_t ** head, const size_t filecount, const char * files[], int * offset, const char verbosity){
    if (fd < 0){
        return -1;
    }

    if (!archive || *archive){
        return -1;
    }

    if (filecount && !files){
        return -1;
    }

    // add new data
    struct tar_t ** tar = archive;  // current entry
    char buf[512];              // one block buffer
	unsigned int i;
    for(i = 0; i < filecount; i++){
        *tar = malloc(sizeof(struct tar_t));

        // stat file
        if (format_tar_data(*tar, files[i], verbosity) < 0){
            WRITE_ERROR(stderr, "Error: Failed to stat %s\n", files[i]);
        }

        if (!i){
            *archive = *tar;  // store first address
        }

        (*tar) -> begin = *offset;

        // write different data depending on file type
        if ((*tar) -> type == DIRECTORY){
            // save parent directory name (source will change)
            const size_t len = strlen((*tar) -> name);
            char * parent = calloc(len + 1, sizeof(char));
            strncpy(parent, (*tar) -> name, len);

            // add a '/' character to the end
            if ((len < 99) && ((*tar) -> name[len - 1] != '/')){
                (*tar) -> name[len] = '/';
                (*tar) -> name[len + 1] = '\0';
                calculate_checksum((*tar));
            }

            V_PRINT(stdout, "%s\n", (*tar) -> name);

            // write metadata to (*tar) file
            if (write_size(fd, (*tar) -> block, 512) != 512){
                WRITE_ERROR(stderr, "Error: Failed to write metadata to archive\n");
            }

            // go through directory
            DIR * d = opendir(parent);
            if (!d){
                WRITE_ERROR(stderr, "Error: Cannot read directory %s\n", parent);
            }

            struct dirent * dir;
            while ((dir = readdir(d))){
                // if not special directories . and ..
                const size_t sublen = strlen(dir -> d_name);
                if (strncmp(dir -> d_name, ".", sublen) && strncmp(dir -> d_name, "..", sublen)){
                    char * path = calloc(len + sublen + 2, sizeof(char));
                    sprintf(path, "%s/%s", parent, dir -> d_name);

                    // recursively write each subdirectory
                    if (write_entries(fd, &((*tar) -> next), head, 1, (const char **) &path, offset, verbosity) < 0){
                        WRITE_ERROR(stderr, "Error: Recurse error\n");
                    }

                    // go to end of new data
                    while ((*tar) -> next){
                        tar = &((*tar) -> next);
                    }

                    free(path);
                }
            }

            free(parent);
            closedir(d);
        }
        else{ // if (((*tar) -> type == REGULAR) || ((*tar) -> type == NORMAL) || ((*tar) -> type == CONTIGUOUS) || ((*tar) -> type == SYMLINK) || ((*tar) -> type == CHAR) || ((*tar) -> type == BLOCK) || ((*tar) -> type == FIFO)){
            V_PRINT(stdout, "%s\n", (*tar) -> name);

            char tarred = 0;   // whether or not the file has already been put into the archive
            if (((*tar) -> type == REGULAR) || ((*tar) -> type == NORMAL) || ((*tar) -> type == CONTIGUOUS) || ((*tar) -> type == SYMLINK)){
                struct tar_t * found = exists(*head, files[i], 1);
                tarred = (found != (*tar));

                // if file has already been included, modify the header
                if (tarred){
                    // change type to hard link
                    (*tar) -> type = HARDLINK;

                    // change link name to (*tar)red file name (both are the same)
                    strncpy((*tar) -> link_name, (*tar) -> name, 100);

                    // change size to 0
                    strncpy((*tar) -> size, "00000000000", 11);

                    // recalculate checksum
                    calculate_checksum((*tar));
                }
            }

            // write metadata to (*tar) file
            if (write_size(fd, (*tar) -> block, 512) != 512){
                WRITE_ERROR(stderr, "Error: Failed to write metadata to archive\n");
            }

            if (((*tar) -> type == REGULAR) || ((*tar) -> type == NORMAL) || ((*tar) -> type == CONTIGUOUS)){
                // if the file isn't already in the tar file, copy the contents in
                if (!tarred){
                    int f = open((*tar) -> name, O_RDONLY);
                    if (f < 0){
                        WRITE_ERROR(stderr, "Error: Could not open %s\n", files[i]);
                    }

                    int r = 0;
                    while ((r = read_size(f, buf, 512)) > 0){
                        if (write_size(fd, buf, r) != r){
                            fprintf(stderr, "[2017.05.26] Error: Could not write to archive: %s\n", strerror(rc));
                        }
                    }

                    close(f);
                }
            }

            // pad data to fill block
            const unsigned int size = oct2uint((*tar) -> size, 11);
            const unsigned int pad = 512 - size % 512;
            if (pad != 512){
				unsigned int j;
                for(j = 0; j < pad; j++){
                    if (write_size(fd, "\0", 1) != 1){
                        WRITE_ERROR(stderr, "Error: Could not write padding data\n");
                    }
                }
                *offset += pad;
            }
            *offset += size;
            tar = &((*tar) -> next);
        }

        // add metadata size
        *offset += 512;
    }

    return 0;
}

int write_end_data(const int fd, int size, const char verbosity){
    if (fd < 0){
        return -1;
    }

    // complete current record
    const int pad = RECORDSIZE - (size % RECORDSIZE);
	int i;
    for(i = 0; i < pad; i++){
        if (write(fd, "\0", 1) != 1){
            fprintf(stderr, "[2017.05.26] Error: Unable to close tar file\n");
            return -1;
        }
    }

    // if the current record does not have 2 blocks of zeros, add a whole other record
    if (pad < (2 * BLOCKSIZE)){
		int i;
        for(i = 0; i < RECORDSIZE; i++){
            if (write(fd, "\0", 1) != 1){
                fprintf(stderr, "[2017.05.26] Error: Unable to close tar file\n");
                return -1;
            }
        }
        return pad + RECORDSIZE;
    }

    return pad;
}

int check_match(struct tar_t * entry, const size_t filecount, const char * bad, const char * files[]){
    if (!entry){
        return -1;
    }

    if (!filecount){
        return 0;
    }

    if (filecount && !files){
        return -1;
    }

	size_t i;
    for(i = 0; i < filecount; i++){
        if (!bad[i]){
            if (!strncmp(entry -> name, files[i], MAX(strlen(entry -> name), strlen(files[i])) + 1)){
                return i + 1;
            }
        }
    }

    return 0;
}

int read_size(int fd, char * buf, int size){
    int got = 0, rc;
    while ((got < size) && ((rc = read(fd, buf + got, size - got)) > 0)){
        got += rc;
    }
    return got;
}

int write_size(int fd, char * buf, int size){
    int wrote = 0, rc;
    while ((wrote < size) && ((rc = write(fd, buf + wrote, size - wrote)) > 0)){
        wrote += rc;
    }
    return wrote;
}

unsigned int oct2uint(char * oct, unsigned int size){
    unsigned int out = 0;
    int i = 0;
    while ((i < size) && oct[i]){
        out = (out << 3) | (unsigned int) (oct[i++] - '0');
    }
    return out;
}

int iszeroed(char * buf, size_t size){
	size_t i;
    for(i = 0; i < size; buf++, i++){
        if (* (char *) buf){
            return 0;
        }
    }
    return 1;
}

int recursive_mkdir(const char * dir, const unsigned int mode, const char verbosity){
    int rc = 0;
    const size_t len = strlen(dir);

    ////////////////////////////////////////////////////////////
    struct stat sb;
    ////////////////////////////////////////////////////////////
    if (!len){
        return 0;
    }

    char * path = calloc(len + 1, sizeof(char));
    strncpy(path, dir, len);

    // remove last '/'
    if (path[len - 1] ==  '/'){
       path[len - 1] = 0;
    }

    // all subsequent directories do not exist
    char * p;
    for(p = path + 1; *p; p++){
        if (*p == '/'){
            *p = '\0';

            int nResult = access(path, 0 );
			if( nResult == 0 )
			{
				fprintf(stderr,"[2017.05.26]-1 recursive_mkdir >> %s directory is exist\n", path );
			}
			else if( nResult == -1 )
			{
				fprintf(stderr,"[2017.05.26]-1 recursive_mkdir >> %s directory created\n", path );
				if ((rc = mkdir(path, mode?mode:DEFAULT_DIR_MODE))){
					fprintf(stderr, "[2017.05.26] Error[1]: Could not create directory %s: %s\n", path, strerror(rc));
				}
			}
            *p = '/';
        }
    }

    int nResult = access(path, 0 );
	if( nResult == 0 )
	{
		fprintf(stderr,"[2017.05.26]-2 recursive_mkdir >> %s directory is exist\n", path );
	}
	else if( nResult == -1 )
	{
		fprintf(stderr,"[2017.05.26]-2 recursive_mkdir >> %s directory created\n", path );
		if ((rc = mkdir(path, mode?mode:DEFAULT_DIR_MODE))){
			fprintf(stderr, "[2017.05.26] Error[1]: Could not create directory %s: %s\n", path, strerror(rc));
		}
	}
	//////////////////////////////////////////////////////////////////////

    free(path);
    return 0;
}

int untar_all(char *source_file, char *destination_path)
{
  int rc = 0;
  char verbosity = 0;
  const char * filename = source_file;
  const char ** files = (const char **) &destination_path;

  struct tar_t * archive = NULL;
  int fd = -1;

	// open existing file
	if ((fd = open(filename, O_RDWR)) < 0){
		printf("[2017.05.26] Error: Unable to open file %s\n", filename);
		return -1;
	}

	// read in data
	if (tar_read(fd, &archive, 0) < 0){
		tar_free(archive);
		close(fd);          // don't bother checking for fd < 0
		printf("[2017.05.26] Error: don't bother checking for fd <0\n");
		return -1;
	}

	if (tar_extract(fd, archive, 0, files, destination_path, verbosity) < 0) {
		printf("[2017.05.26] Exiting with error due to previous error\n");
		rc = -1;
	}

  tar_free(archive);
  close(fd);          // don't bother checking for fd < 0
  return rc;
}

