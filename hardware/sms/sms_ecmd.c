/*
 * Copyright (c) 2009 by Benjamin Sackenreuter <benson@zerties.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * For more information on the GPL, please go to:
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <util/delay.h>
#include "core/debug.h"
#include "sms_encoding.h"
#include "sms.h"
#include "sms_ecmd.h"
#include "protocols/ecmd/parser.h"
#include "protocols/ecmd/ecmd-base.h"

#define DEBUG_ENABLE
#define DEBUG_REC
/* define max sms length */
//#define DATA_LEN ((160*7)/ 8) * 2 + 1
#define WRITE_BUFF 119

static char write_buffer[WRITE_BUFF + 1];	
static uint8_t text[50];
char nummer[20];

static void read_sms(uint8_t *pdu) 
{
    uint16_t j = 2;
	uint8_t plain_len;
    uint8_t current_byte;
    uint8_t temp = 0;
    uint8_t i = 0, shift = 7;
	
	sscanf((char *) pdu, "%02hhX", &plain_len);
    while (i < plain_len) {
        sscanf((char *) pdu + j, "%02hhX", &current_byte);
        text[i++] = (temp | ((((uint8_t) current_byte) << (7 - shift)) & 0x7F));
        temp = current_byte >> shift;
        
		j += 2;
        shift --;
        if (shift == 0) {
            shift = 7;
            text[i++] = temp;
            temp = 0;
        }
    }
	text[i] = '\0';
 #ifdef DEBUG_REC 	
	debug_printf("plain: %s, %d\r\n", text, i);
#endif
}




uint8_t pdu_parser(uint8_t *pdu) 
{
	uint8_t i = 3;
	uint8_t ret_val = 1;
	int16_t write_len = 0;
	uint16_t reply_len = 0;

	debug_printf("pdu_parser called: %d\n", pdu[i]);
	if (pdu[i++] == '+') {
		debug_printf("pdu_parser called: %d\n", pdu[i]);
		if (!strncmp((char *) pdu + i, "CMGL", 4)) {	
			#ifdef DEBUG_ENABLE
			debug_printf("here we go!!\r\n");
			#endif
			uint8_t smsc_len, sender_len;
			if (reply_len == 0) {
				while (pdu[++i] != '\n');
				i ++;
				/* get length of smsc number */
				sscanf((char *) pdu + i, "%02hhX", &smsc_len);
				/* we ignore some flags "0000" */
				i += (smsc_len * 2) + 4;
				/* get data length */
				sscanf((char *) pdu + i, "%02hhX", &sender_len);
				/* get number of message sender */
				strncpy(nummer, (char *) pdu + i, sender_len + 5);
				nummer[sender_len + 5] = '\0';
				i = i + sender_len + 5 + 18;
				read_sms((uint8_t *) pdu + i);
			}
			
			debug_printf("ecmd: %s\n", text);
			do {
				if (reply_len <= (int16_t)sizeof(write_buffer)) {
					write_len = ecmd_parse_command((char *) text, write_buffer + reply_len, sizeof(write_buffer) - reply_len);
					debug_printf("ret_len ecmd parser: %d\r\n", write_len);
					if (is_ECMD_AGAIN(write_len)) {
						reply_len += -(10 + write_len);
						*(write_buffer + reply_len) = ' ';
						reply_len++;
					} 
					else if (write_len > 0) {
						// everything received, we terminate the string if it is longer
						debug_printf("finished: \"%s\"\n", write_buffer);
						ret_val = 0;
						reply_len += write_len;
						break;
					}
					debug_printf("%s\n", write_buffer);
				}
				else {
					break;
				}
			} while (write_len <= 0);
			write_buffer[reply_len] = '\0';
			sms_send((uint8_t *) nummer, (uint8_t *) write_buffer, NULL, 1);
		}
	}
	return ret_val;
}

