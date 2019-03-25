
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <limits.h>

typedef struct {
	int16_t someField[24];
	int32_t someInt;
	int16_t productName[256];
	int16_t firmwareName[256];
	int32_t partitionCount;
	int32_t partitionsListStart;
	int32_t someIntFields1[5];
	int16_t productName2[50];
	int16_t someIntFields2[6];
	int16_t someIntFields3[2];
} PacHeader;

typedef struct {
	uint32_t length;
	int16_t partitionName[256];
	int16_t fileName[512];
	uint32_t partitionSize;
	int32_t someFileds1[2];
	uint32_t partitionAddrInPac;
	int32_t someFileds2[3];
	int32_t dataArray[];
} PartitionHeader;

void getString(int16_t* baseString, char* resString) {
	if(*baseString == 0) {
		*resString = 0;
		return;
	}
	int length = 0;
	do {
		*resString = 0xFF & *baseString;
		resString++;
		baseString++;
		if(++length > 256)
			break;
	} while(baseString > 0);
	*resString = 0;
}

int main(int argc, char** argv) {
	char tmp[PATH_MAX];
	char* outdir = "./";
	char* filename = NULL;

	extern char *optarg;
	extern int optind;
	int c, err = 0;
	int fflag=0;
	static char usage[] = "usage: %s -f filename [-o outdir]\n";

	while ((c = getopt(argc, argv, "f:o:")) != -1) switch (c) {
		case 'f':
			fflag = 1;
			filename = optarg;
			break;
		case 'o':
			outdir = optarg;
			break;
		case '?':
			err = 1;
			break;
	}
	if (fflag == 0) {
		fprintf(stderr, "%s: missing -f option\n", argv[0]);
		fprintf(stderr, usage, argv[0]);
		exit(1);
	} else if (err) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}
	
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		printf("file %s not found\n", filename);
		exit(EXIT_FAILURE);
	}
	
//	fseek(fd, 0, SEEK_END);
//	int firmwareSize = (fd);
//	fseek(fd, 0, SEEK_SET);
	struct stat st;
	stat(filename, &st);
	int firmwareSize = st.st_size;
	if(firmwareSize < sizeof(PacHeader)) {
		printf("file %s is not firmware\n", filename);
		exit(EXIT_FAILURE);
	}
	
	PacHeader pacHeader;
	size_t rb = read(fd, &pacHeader, sizeof(PacHeader));
	if(rb <= 0) {
		printf("Error while parsing PAC header\n");
		exit(EXIT_FAILURE);
	}

	char buffer[256];
	char buffer1[256];
	getString(pacHeader.firmwareName, buffer);
	printf("Firmware name: %s\n", buffer);
	uint32_t curPos = pacHeader.partitionsListStart;
	PartitionHeader** partHeaders = malloc(sizeof(PartitionHeader**)*pacHeader.partitionCount);
	int i;
	for(i = 0; i < pacHeader.partitionCount; i++) {
		lseek(fd, curPos, SEEK_SET);
		uint32_t length;
		rb = read(fd, &length, sizeof(uint32_t));
		if(rb <= 0) {
			printf("Partition header error\n");
			exit(EXIT_FAILURE);
		}
		partHeaders[i] = malloc(length);
		lseek(fd, curPos, SEEK_SET);
		curPos += length;
		rb = read(fd, partHeaders[i], length);
		if(rb <= 0) {
			printf("Partition header error\n");
			exit(EXIT_FAILURE);
		}
		getString(partHeaders[i]->partitionName, buffer);
		getString(partHeaders[i]->fileName, buffer1);
		printf("Partition name: %s\n\twith file name: %s\n\twith size %u\n", buffer, buffer1, partHeaders[i]->partitionSize);
	}
	
	if (stat(outdir, &st) == -1) {
		mkdir(outdir, 0755);
	}

	for(i = 0; i < pacHeader.partitionCount; i++) {
		if(partHeaders[i]->partitionSize == 0) {
			free(partHeaders[i]);
			continue;
		}
		lseek(fd, partHeaders[i]->partitionAddrInPac, SEEK_SET);
		getString(partHeaders[i]->fileName, buffer);
		remove(buffer);
		sprintf(tmp, "%s/%s", outdir, buffer);
		int fd_new = open(tmp, O_WRONLY | O_CREAT, 0666);
		printf("Extract %s\n", buffer);
		uint32_t dataSizeLeft = partHeaders[i]->partitionSize;
		while(dataSizeLeft > 0) {
			uint32_t copyLength = (dataSizeLeft > 256) ? 256 : dataSizeLeft;
			dataSizeLeft -= copyLength;
			rb = read(fd, buffer, copyLength);
			if(rb != copyLength) {
				printf("Partition image extraction error\n");
				exit(EXIT_FAILURE);
			}
			rb = write(fd_new, buffer, copyLength);
			if(rb != copyLength) {
				printf("Partition image extraction error\n");
				exit(EXIT_FAILURE);
			}
#if defined(__CYGWIN__)
			printf("\r\t%02llu%%", (uint64_t)100 - (uint64_t)100*dataSizeLeft/partHeaders[i]->partitionSize);
#else
			printf("\r\t%02zu%%", (uint64_t)100 - (uint64_t)100*dataSizeLeft/partHeaders[i]->partitionSize);
#endif
		}
		printf("\n");
		close(fd_new);
		free(partHeaders[i]);
	}
	free(partHeaders);
	close(fd);
	
	return EXIT_SUCCESS;
}