/**
  * GreenPois0n Apparition - mbdb.h
  * Copyright (C) 2010 Chronic-Dev Team
  * Copyright (C) 2010 Joshua Hill
  * Copyright (C) 2012 Hanéne Samara
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef MBDB_H
#define MBDB_H

#include "mbdb_record.h"

#define MBDB_MAGIC "\x6d\x62\x64\x62\x05\x00"


typedef struct mbdb_header {
    unsigned char magic[6];		       // 'mbdb\5\0'
} mbdb_header_t;

typedef struct mbdb_t {
    unsigned int size;
    unsigned char* data;
    mbdb_header_t* header;
    int num_records;
    mbdb_record_t** records;
} mbdb_t;

extern mbdb_t* apparition_mbdb;

mbdb_t* mbdb_create();
mbdb_t* mbdb_open(char* file);
mbdb_t* mbdb_parse(unsigned char* data, unsigned int size);
mbdb_record_t* mbdb_get_record(mbdb_t* mbdb, unsigned int offset);
void mbdb_free(mbdb_t* mbdb);

#endif

