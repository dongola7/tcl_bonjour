/*
Copyright (c) 2006, Blair Kitchen All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer. Redistributions in binary
form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials
provided with the distribution. Neither the name of Blair Kitchen nor
the names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.
*/

#include <dns_sd.h>
#include <string.h>
#include <tcl.h>

#include "bonjour.h"
#include "txt_record.h"

///////////////////////////////////////////////////////////
// Function to convert a txt record to a Tcl list.
// The list will be of the format {key val ?key val? ...}
///////////////////////////////////////////////////////////
void txt2list(
   uint16_t txtLen,        // TXT record len
   const void *txtRecord,  // TXT record
   Tcl_Obj **tclList       // Pointer to uninitialized Tcl object
) {
   Tcl_Obj *result = Tcl_NewListObj(0, NULL);

   uint16_t numRecords = TXTRecordGetCount(txtLen, txtRecord);
   uint16_t i;
   for(i = 0; i < numRecords; i++)
   {
      char key[256];
      uint8_t valueLen;
      const void *value;

      TXTRecordGetItemAtIndex(txtLen, txtRecord,
         i, 255, key,
         &valueLen, &value);
      
      Tcl_ListObjAppendElement(NULL,
         result, Tcl_NewStringObj(key, strlen(key)));
      Tcl_ListObjAppendElement(NULL,
         result, Tcl_NewByteArrayObj(value, valueLen));
   }

   *tclList = result;
}

///////////////////////////////////////////////////////////
// Function to convert a Tcl list of the form 
// {key val ?key val?...} into a TXT record.
///////////////////////////////////////////////////////////
void list2txt(
   Tcl_Obj *tclList,    // Tcl list
   uint16_t *txtLen,    // length of TXT record
   void **txtRecord     // TXT record (must be deallocated by caller)
) {

   int listLen;
   TXTRecordRef txtRecordRef;

   Tcl_ListObjLength(NULL, tclList, &listLen);

   // initialize the TXT record
   TXTRecordCreate(&txtRecordRef, 0, NULL);

   int i;
   for(i = 0; i < listLen; i+=2)
   {
      Tcl_Obj *keyObj, *valueObj;

      Tcl_ListObjIndex(NULL, tclList, i, &keyObj);
      Tcl_ListObjIndex(NULL, tclList, i+1, &valueObj);

      char *key = Tcl_GetStringFromObj(keyObj, NULL);
      int valueLen;
      unsigned char *value = Tcl_GetByteArrayFromObj(valueObj, &valueLen);

      TXTRecordSetValue(&txtRecordRef, key, (uint8_t)valueLen, value);
   }

   *txtLen = TXTRecordGetLength(&txtRecordRef);
   *txtRecord = (char *)ckalloc(*txtLen);
   memcpy(*txtRecord, TXTRecordGetBytesPtr(&txtRecordRef), *txtLen);
   TXTRecordDeallocate(&txtRecordRef);
}
