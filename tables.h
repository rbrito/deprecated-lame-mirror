/*
** Copyright (C) 1999 Albert L. Faber
**  
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TABLES_H_INCLUDED
#define TABLES_H_INCLUDED
#include "encoder.h"
#include "machine.h"

extern const FLOAT8  psy_data [];

#define HUFFBITS unsigned long int
#define HTN	34
 
struct huffcodetab {
    size_t                 xlen; 	/*max. x-index+			      	*/ 
    size_t                 linmax;	/*max number to be stored in linbits	*/
    const HUFFBITS*        table;	/*pointer to array[xlen][ylen]		*/
    const unsigned char*   hlen;	/*pointer to array[xlen][ylen]		*/
};

extern const struct huffcodetab  ht [HTN];/* global memory block		*/
				/* array of all huffcodtable headers	*/
				/* 0..31 Huffman code table 0..31	*/
				/* 32,33 count1-tables			*/


extern const struct huffcodetab  ht_takehiro [HTN];/* global memory block		*/
				/* array of all huffcodtable headers	*/
				/* 0..31 Huffman code table 0..31	*/
				/* 32,33 count1-tables			*/


extern const unsigned char t32l [];
extern const unsigned char t33l [];

#endif
