//
// Created by 11197 on 25-1-2.
//

#include "mediabase.h"


NaluStruct::NaluStruct(int size) {
    this->size = size;
    type = 0;
    data = (unsigned char *)malloc(size * sizeof(char));
}

NaluStruct::NaluStruct(const unsigned char *data, int size) {
    this->size = size;
    type = data[4] & 0x1f;
    // type = data[0] & 0x1f;
    this->data = (unsigned char *)malloc(size * sizeof(char));
    memcpy(this->data, data, size);
}

NaluStruct::~NaluStruct() {
    if (data) {
        free(data);
        data = nullptr;
    }
}


