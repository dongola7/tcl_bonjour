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

#include "bonjour.h"

////////////////////////////////////////////////////
// Support structures
////////////////////////////////////////////////////

// information on a browse operation currently in
// progress
typedef struct {
   DNSServiceRef sdRef; // the service discovery reference
   char *regtype;       // the regtype being discovered
   Tcl_Obj *callback;   // the callback script
   Tcl_Interp *interp;  // interpreter in which to execute the
                        // callback
} active_browse;

// stores active_browse structures hashed on the regtype being
// browsed
static Tcl_HashTable browseRegistrations;

////////////////////////////////////////////////////
// Private function prototypes
////////////////////////////////////////////////////

static int bonjour_browse(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
);
static int bonjour_browse_start(
   Tcl_Interp *interp,
   const char *const regtype,
   Tcl_Obj *const callbackScript,
   Tcl_HashTable *browseRegistrations
);
static int bonjour_browse_stop(
   Tcl_Interp *interp,
   const char *const regtype,
   Tcl_HashTable *browseRegistrations
);
static void bonjour_browse_callback(
   DNSServiceRef sdRef,
   DNSServiceFlags flags,
   uint32_t interfaceIndex,
   DNSServiceErrorType errorCode,
   const char *const serviceName,
   const char *const replyType,
   const char *const replyDomain,
   void *context
);
static int bonjour_browse_cleanup(
   ClientData clientData
);

////////////////////////////////////////////////////
// Function to initialize browse related stuff
////////////////////////////////////////////////////
int Browse_Init(
   Tcl_Interp *interp
) {

   // initialize the has table
   Tcl_InitHashTable(&browseRegistrations, TCL_STRING_KEYS);

   // register commands
   Tcl_CreateObjCommand(
      interp, "::bonjour::browse", bonjour_browse,
      &browseRegistrations, NULL
   );

   // create an exit handler for cleanup
   Tcl_CreateExitHandler(
      (Tcl_ExitProc *)bonjour_browse_cleanup,
      &browseRegistrations
   );

   return TCL_OK;
}

////////////////////////////////////////////////////
// ::bonjour::browse command
////////////////////////////////////////////////////
static int bonjour_browse(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
) {
   char *subcommands[] = {
      "start", "stop", NULL
   };
   const char *regtype = NULL;
   int result = TCL_OK;
   int cmdIndex;
   Tcl_HashTable *browseRegistrations;

   browseRegistrations = (Tcl_HashTable *)clientData;

   if(objc < 2) {
      Tcl_WrongNumArgs(interp, 1, objv, "<sub-command> <args>");
      return(TCL_ERROR);
   }

   if(Tcl_GetIndexFromObj(
         interp, objv[1], (const char **)subcommands, 
         "subcommand", 0, &cmdIndex
      ) != TCL_OK) {
      return(TCL_ERROR);
   }

   switch(cmdIndex) {
   case 0: // start
      if(objc != 4) {
         Tcl_WrongNumArgs(interp, 2, objv, "<regtype> <callback>");
         return(TCL_ERROR);
      }

      regtype = Tcl_GetString(objv[2]);
      result = 
         bonjour_browse_start(
            interp, regtype, objv[3], browseRegistrations
         );
         
      return(result);
      break;
   case 1: // stop
      if(objc != 3) {
         Tcl_WrongNumArgs(interp, 2, objv, "<regtype>");
         return(TCL_ERROR);
      }

      regtype = Tcl_GetString(objv[2]);
      result = 
         bonjour_browse_stop(interp, regtype, browseRegistrations);
      break;
   default:
      Tcl_SetResult(interp, "Unknown option", TCL_STATIC);
      result = TCL_ERROR;
   } // end switch(cmdIndex)

   return(result);
}

////////////////////////////////////////////////////
// start browsing for a service type
////////////////////////////////////////////////////
static int bonjour_browse_start(
   Tcl_Interp *interp,
   const char *const regtype,
   Tcl_Obj *const callbackScript,
   Tcl_HashTable *browseRegistrations
) {
   active_browse *activeBrowse = NULL;
   Tcl_HashEntry *hashEntry = NULL;
   int newFlag;

   // attempt to create an entry in the hash table
   // for this regtype
   hashEntry = 
      Tcl_CreateHashEntry(browseRegistrations, regtype, &newFlag);
   // if an entry already exists, return an error
   if(!newFlag) {
      Tcl_Obj *errorMsg = Tcl_NewStringObj(NULL, 0);
      Tcl_AppendStringsToObj(
         errorMsg, "regtype ", regtype, " is already being browsed", NULL);
      Tcl_SetObjResult(interp, errorMsg);
      return(TCL_ERROR);
   }

   // allocate the active_browse structure for this
   // regtype
   activeBrowse = (active_browse *)ckalloc(sizeof(active_browse));
   activeBrowse->regtype = (char *)ckalloc(strlen(regtype) + 1);
   strcpy(activeBrowse->regtype, regtype);
   activeBrowse->callback = callbackScript;
   Tcl_IncrRefCount(activeBrowse->callback);
   activeBrowse->interp = interp;

   // store the active_browse structure in the hash entry
   Tcl_SetHashValue(hashEntry, activeBrowse);

   // call DNSServiceBrowse
   DNSServiceErrorType error =
      DNSServiceBrowse(
         &activeBrowse->sdRef,
         0, 0, regtype, NULL,
         bonjour_browse_callback,
         activeBrowse);
   if(error != kDNSServiceErr_NoError)
   {
      ckfree(activeBrowse->regtype);
      ckfree((void *)activeBrowse);
      Tcl_DecrRefCount(activeBrowse->callback);
      Tcl_DeleteHashEntry(hashEntry);

      Tcl_SetObjResult(interp, create_dnsservice_error(interp, "DNSServiceBrowse", error));
      return TCL_ERROR;
   }

   // retrieve the socket being used for the browse operation
   // and register a file handler so that we know when
   // there is data to be read
   Tcl_CreateFileHandler(
      DNSServiceRefSockFD(activeBrowse->sdRef),
      TCL_READABLE,
      bonjour_tcl_callback,
      activeBrowse->sdRef);

   return(TCL_OK);
}

////////////////////////////////////////////////////
// stop browsing for a service type
////////////////////////////////////////////////////
static int bonjour_browse_stop(
   Tcl_Interp *interp,
   const char *const regtype,
   Tcl_HashTable *browseRegistrations
) {
   active_browse *activeBrowse = NULL;
   Tcl_HashEntry *hashEntry = NULL;
   
   // retrieve the hash entry for this regtype
   // from the hash table
   hashEntry = Tcl_FindHashEntry(browseRegistrations, regtype);

   // if a valid hash entry was found, clean it up
   if(hashEntry) {
      activeBrowse = (active_browse *)Tcl_GetHashValue(hashEntry);

      // remove the file handler
      Tcl_DeleteFileHandler(DNSServiceRefSockFD(activeBrowse->sdRef));

      // deallocate the browse service reference
      DNSServiceRefDeallocate(activeBrowse->sdRef);

      // clean up the memory used by activeBrowse
      ckfree(activeBrowse->regtype);
      ckfree((void *)activeBrowse);

      // let Tcl know the callback object is no longer
      // in use
      Tcl_DecrRefCount(activeBrowse->callback);

      // deallocate the hash entry
      Tcl_DeleteHashEntry(hashEntry);
   }

   return(TCL_OK);
}

////////////////////////////////////////////////////
// called when a service browse result is received.
// executes the appropriate Tcl callback to let
// the application know what has happened
////////////////////////////////////////////////////
static void bonjour_browse_callback(
   DNSServiceRef sdRef,
   DNSServiceFlags flags,
   uint32_t interfaceIndex,
   DNSServiceErrorType errorCode,
   const char *const serviceName,
   const char *const replyType,
   const char *const replyDomain,
   void *context
) {
   active_browse *activeBrowse = NULL;
   Tcl_Obj *callback;
   int result;

   activeBrowse = (active_browse *)context;

   // begin creating the callback as a list
   callback = Tcl_NewListObj(0, NULL);
   Tcl_ListObjAppendList(NULL, callback, activeBrowse->callback);

   if(errorCode == kDNSServiceErr_NoError) {
      // determine whether a service is being
      // added or removed
      if(flags & kDNSServiceFlagsAdd) {
         Tcl_ListObjAppendElement(
            activeBrowse->interp,
            callback,
            Tcl_NewStringObj("add", 3));
      }
      else {
         Tcl_ListObjAppendElement(
            activeBrowse->interp,
            callback,
            Tcl_NewStringObj("remove", 6));
      }

      // append the service name and domain
      Tcl_ListObjAppendElement(
         activeBrowse->interp,
         callback,
         Tcl_NewStringObj(serviceName, -1));
      Tcl_ListObjAppendElement(
         activeBrowse->interp,
         callback,
         Tcl_NewStringObj(replyDomain, -1));

      // evaluate the callback
      result = Tcl_GlobalEvalObj(activeBrowse->interp, callback);
   } // end if no error
   else {
      // store an appropriate error message in the interpreter
      Tcl_SetObjResult(activeBrowse->interp, 
         create_dnsservice_error(activeBrowse->interp, "DNSServiceBrowseReply", errorCode));
      result = TCL_ERROR;
   }

   if(result == TCL_ERROR) {
      Tcl_BackgroundError(activeBrowse->interp);
   }
}

////////////////////////////////////////////////////
// cleanup any leftover browsing
////////////////////////////////////////////////////
static int bonjour_browse_cleanup(
   ClientData clientData
) {
   Tcl_HashTable *browseRegistrations = NULL;
   Tcl_HashEntry *hashEntry = NULL;
   Tcl_HashSearch searchToken;
   active_browse *activeBrowse = NULL;

   browseRegistrations = (Tcl_HashTable *)clientData;

   // run through the remaining entries in the hash table
   for(hashEntry = Tcl_FirstHashEntry(browseRegistrations,
                                      &searchToken);
       hashEntry != NULL;
       hashEntry = Tcl_NextHashEntry(&searchToken)) {

      activeBrowse = (active_browse *)Tcl_GetHashValue(hashEntry);

      // remove the file handler
      Tcl_DeleteFileHandler(DNSServiceRefSockFD(activeBrowse->sdRef));

      // deallocate the browse service reference
      DNSServiceRefDeallocate(activeBrowse->sdRef);

      // clean up the memory used by activeBrowse
      ckfree(activeBrowse->regtype);
      ckfree((void *)activeBrowse);

      // let Tcl know the callback object is no longer
      // in use
      Tcl_DecrRefCount(activeBrowse->callback);

      // deallocate the hash entry
      Tcl_DeleteHashEntry(hashEntry);
   } // end loop over hash entries

   Tcl_DeleteHashTable(browseRegistrations);

   return TCL_OK;
}
