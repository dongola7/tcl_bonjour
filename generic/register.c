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

#include <string.h>
#include <tcl.h>

#include <dns_sd.h>

#include "txt_record.h"

////////////////////////////////////////////////////
// Support structures
////////////////////////////////////////////////////

// information on a registration currently in progress
typedef struct {
   DNSServiceRef sdRef; // the service discovery reference
   char *regtype;       // the regtype registered
} active_registration;

// stores active_registration structures hashed on the
// regtype being registered
static Tcl_HashTable registerRegistrations;

////////////////////////////////////////////////////
// Private function prototypes
////////////////////////////////////////////////////

static int bonjour_register(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
);
static int bonjour_register_cleanup(
   ClientData clientData
);

////////////////////////////////////////////////////
// Function to initialize register related stuff
////////////////////////////////////////////////////
int Register_Init(
   Tcl_Interp *interp
) {
   
   // initialize the hash table
   Tcl_InitHashTable(&registerRegistrations, TCL_STRING_KEYS);

   // register our commands
   Tcl_CreateObjCommand(
      interp, "::bonjour::register", bonjour_register,
      &registerRegistrations, NULL
   );

   // create an exit handler for cleanup
   Tcl_CreateExitHandler(
      (Tcl_ExitProc *)bonjour_register_cleanup,
      &registerRegistrations
   );


   return TCL_OK;
}

////////////////////////////////////////////////////
// ::bonjour::register command
////////////////////////////////////////////////////
static int bonjour_register(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
) {
   const char *regtype;
   unsigned int port;
   active_registration *activeRegister;
   Tcl_HashTable *registerRegistrations = (Tcl_HashTable *)clientData;
   Tcl_HashEntry *hashEntry;
   int newFlag = 0;
   uint16_t txtLen = 0;
   void *txtRecord = NULL;

   if(objc < 3 || objc > 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "<regtype> <port> ?txt-record-list?");
      return(TCL_ERROR);
   }

   // retrieve the registration type (service name)
   regtype = Tcl_GetString(objv[1]);

   // retrieve the port number
   if(Tcl_GetIntFromObj(interp, objv[2], (int *)&port) != TCL_OK)
      return TCL_ERROR;
   
   // retrieve the txt record list, if applicable
   if(objc > 3)
   {
      list2txt(objv[3], &txtLen, &txtRecord);
   }

   // attempt to create an entry in the hash table
   // for this regtype
   hashEntry = 
      Tcl_CreateHashEntry(registerRegistrations, regtype, &newFlag);
   // if an entry already exists, return an error
   if(!newFlag) {
      Tcl_Obj *errorMsg = Tcl_NewStringObj(NULL, 0);
      Tcl_AppendStringsToObj(
         errorMsg, "regtype ", regtype, " is already registered", NULL);
      Tcl_SetObjResult(interp, errorMsg);
      return(TCL_ERROR);
   }

   // create the activeRegister structure
   activeRegister = (active_registration *)ckalloc(sizeof(active_registration));
   activeRegister->regtype = (char *)ckalloc(strlen(regtype) + 1);
   strcpy(activeRegister->regtype, regtype);

   // store the activeRegister structure
   Tcl_SetHashValue(hashEntry, activeRegister);

   DNSServiceRegister(&activeRegister->sdRef,
                      0, 0,
                      NULL, regtype,
                      NULL, NULL,
                      (uint16_t)port,
                      txtLen, txtRecord, // txt record stuff
                      NULL, NULL); // callback stuff

   // free the txt record
   ckfree(txtRecord);

   return TCL_OK;
}

////////////////////////////////////////////////////
// cleanup any leftover registration
////////////////////////////////////////////////////
static int bonjour_register_cleanup(
   ClientData clientData
) {
   Tcl_HashTable *registerRegistrations = 
      (Tcl_HashTable *)clientData;
   Tcl_HashEntry *hashEntry = NULL;
   Tcl_HashSearch searchToken;
   active_registration *activeRegister = NULL;

   // run through the remaining entries in the hash table
   for(hashEntry = Tcl_FirstHashEntry(registerRegistrations,
                                      &searchToken);
       hashEntry != NULL;
       hashEntry = Tcl_NextHashEntry(&searchToken)) {

      activeRegister = (active_registration *)Tcl_GetHashValue(hashEntry);

      // deallocate the service reference
      DNSServiceRefDeallocate(activeRegister->sdRef);

      // clean up the memory used by activeRegister
      ckfree(activeRegister->regtype);
      ckfree((void *)activeRegister);

      // deallocate the hash entry
      Tcl_DeleteHashEntry(hashEntry);
   }

   Tcl_DeleteHashTable(registerRegistrations);

   return(TCL_OK);
}


