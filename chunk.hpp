#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <stdio.h>
#include <stdlib.h>
#include "constants.h"

class Chunk{

    private:
        char chunk[CHUNK_SIZE+1024];
        char recPack[CHUNK_SIZE+1024];
        char recChunk[CHUNK_SIZE];
        void addPaddingStart();
        void addLimitator(int offset, const char type[]);
        void addTransmissionType(const char type[]);
        void addContentLength(long dataLength, int offset);
        void addFileNameSizeLength(int f_name_length, int offset);
        void addFileName(char f_name[], int offset);
        void addChunkOrderNumber(int offset, int orderNumber);
        void addContent(int offset, char* content);
        void addPaddingEnd(int offset);

        char* getFileName();

    public:
        Chunk(u_char (&content)[CHUNK_SIZE], char fileName[], long size, int order);
        Chunk(char (&content)[CHUNK_SIZE+1024]);
        void getBytes(char* target);
        int getContentLength();
        void getContent(char *content);
        int getOrderNumber();
};

#endif