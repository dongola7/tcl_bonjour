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
#include <arpa/inet.h>
#include <dns_sd.h>

#include "bonjour.h"
#include "txt_record.h"

////////////////////////////////////////////////////
// Support structures
////////////////////////////////////////////////////

// information on a resolve currently in progress
typedef struct {
   DNSServiceRef sdRef; // the service discovery reference
   Tcl_Obj *callback;   // the callback script
   Tcl_Interp *interp;  // interpreter in which to execute the
                        // callback
} active_resolve;

////////////////////////////////////////////////////
// Private function prototypes
////////////////////////////////////////////////////

static int bonjour_resolve(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
);
static int bonjour_resolve_address(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
);
static void bonjour_resolve_callback(
   DNSServiceRef sdRef,
   DNSServiceFlags flags,
   uint32_t interfaceIndex,
   DNSServiceErrorType errorCode,
   const char *fullname,
   const char *hosttarget,
   uint16_t port,
   uint16_t txtLen,
   const char *txtRecord,
   void *context
);
static void bonjour_resolve_address_callback(
   DNSServiceRef sdRef,
   DNSServiceFlags flags,
   uint32_t interfaceIndex,
   DNSServiceErrorType errorCode,
   const char *fullname,
   uint16_t rrtype,
   uint16_t rrclass,
   uint16_t rdlen,
   const void *rdata,
   uint32_t ttl,
   void *context
);

////////////////////////////////////////////////////
// Function to initialize resolve related stuff
////////////////////////////////////////////////////
int Resolve_Init(
   Tcl_Interp *interp
) {

   // register commands
   Tcl_CreateObjCommand(
      interp, "::bonjour::resolve", bonjour_resolve,
      NULL, NULL
   );

   Tcl_CreateObjCommand(
      interp, "::bonjour::resolve_address", bonjour_resolve_address,
      NULL, NULL
   );

   return TCL_OK;
}

////////////////////////////////////////////////////
// ::bonjour::resolve command
////////////////////////////////////////////////////
static int bonjour_resolve(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
) {
   const char *hostname = NULL,
              *regtype = NULL,
              *domain = NULL;
   Tcl_Obj *callbackScript = NULL;
   active_resolve *activeResolve = NULL;

   // check for the appropriate number of arguments
   if(objc != 5) {
      Tcl_WrongNumArgs(interp, 1, objv, "<name> <regtype> <domain> <script>");
      return(TCL_ERROR);
   }

   // retrieve the argument values
   hostname = Tcl_GetString(objv[1]);
   regtype = Tcl_GetString(objv[2]);
   domain = Tcl_GetString(objv[3]);
   callbackScript = Tcl_DuplicateObj(objv[4]);

   // increment the reference count on the callback script
   // since we will be holding onto it until the callback
   // is executed
   Tcl_IncrRefCount(callbackScript);

   // create the active_resolve structure
   activeResolve = (active_resolve *)ckalloc(sizeof(active_resolve));
   activeResolve->callback = callbackScript;
   activeResolve->interp = interp;

   // start the resolution
   DNSServiceErrorType error =
      DNSServiceResolve(
         &activeResolve->sdRef,
         0,
         0,
         hostname,
         regtype,
         domain,
         (DNSServiceResolveReply)bonjour_resolve_callback,
         (void *)activeResolve);

   if(error != kDNSServiceErr_NoError)
   {
      Tcl_DecrRefCount(activeResolve->callback);
      ckfree((void *)activeResolve);

      Tcl_SetObjResult(interp, create_dnsservice_error(interp, "DNSServiceResolve", error));
      return TCL_ERROR;
   }

   // retrieve the socket being used for the resolve operation
   // and register a file handler so that we know when
   // there is data to be read
   Tcl_CreateFileHandler(
      DNSServiceRefSockFD(activeResolve->sdRef),
      TCL_READABLE,
      bonjour_tcl_callback,
      activeResolve->sdRef);

   return(TCL_OK);
}

////////////////////////////////////////////////////
// ::bonjour::resolve_address command
////////////////////////////////////////////////////
static int bonjour_resolve_address(
   ClientData clientData,
   Tcl_Interp *interp,
   int objc,
   Tcl_Obj *const objv[]
) {
   const char *fullname = NULL;
   Tcl_Obj *callbackScript = NULL;
   active_resolve *activeResolve = NULL;

   // check for the appropriate number of arguments
   if(objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "<fullname> <script>");
      return(TCL_ERROR);
   }

   // retrieve the argument values
   fullname = Tcl_GetString(objv[1]);
   callbackScript = Tcl_DuplicateObj(objv[2]);

   // increment the reference count on the callback script
   // since we will be holding onto it until the callback
   // is executed
   Tcl_IncrRefCount(callbackScript);

   // create the active_resolve structure
   activeResolve = (active_resolve *)ckalloc(sizeof(active_resolve));
   activeResolve->callback = callbackScript;
   activeResolve->interp = interp;

   // start the resolution
   DNSServiceErrorType error =
      DNSServiceQueryRecord(
         &activeResolve->sdRef,
         0,
         0,
         fullname,
         kDNSServiceType_A,
         kDNSServiceClass_IN,
         (DNSServiceQueryRecordReply)bonjour_resolve_address_callback,
         (void *)activeResolve);
   if(error != kDNSServiceErr_NoError)
   {
      Tcl_DecrRefCount(activeResolve->callback);
      ckfree((void *)activeResolve);

      Tcl_SetObjResult(interp, create_dnsservice_error(interp, "DNSServiceQueryRecord", error));
      return TCL_ERROR;
   }
      
   // retrieve the socket being used for the resolve operation
   // and register a file handler so that we know when
   // there is data to be read
   Tcl_CreateFileHandler(
      DNSServiceRefSockFD(activeResolve->sdRef),
      TCL_READABLE,
      bonjour_tcl_callback,
      activeResolve->sdRef);

   return(TCL_OK);
}

////////////////////////////////////////////////////
// called when a service resolve result is received.
// executes the appropriate Tcl callback to let
// the application know what has happened
////////////////////////////////////////////////////
static void bonjour_resolve_callback(
   DNSServiceRef sdRef,
   DNSServiceFlags flags,
   uint32_t interfaceIndex,
   DNSServiceErrorType errorCode,
   const char *fullname,
   const char *hosttarget,
   uint16_t port,
   uint16_t txtLen,
   const char *txtRecord,
   void *context
) {
   active_resolve *activeResolve = (active_resolve *)context;
   Tcl_Obj *txtRecordList = NULL;
   int result;

   if(errorCode == kDNSServiceErr_NoError) {
      // append the service name and domain
      Tcl_ListObjAppendElement(
         activeResolve->interp,
         activeResolve->callback,
         Tcl_NewStringObj(fullname, -1));
      Tcl_ListObjAppendElement(
         activeResolve->interp,
         activeResolve->callback,
         Tcl_NewStringObj(hosttarget, -1));
      Tcl_ListObjAppendElement(
         activeResolve->interp,
         activeResolve->callback,
         Tcl_NewIntObj(ntohs(port)));

      // create the TXT record list
      txt2list(txtLen, txtRecord, &txtRecordList);

      Tcl_ListObjAppendElement(
         activeResolve->interp,
         activeResolve->callback,
         txtRecordList);

      // evaluate the callback
      result = Tcl_GlobalEvalObj(activeResolve->interp, 
                                 activeResolve->callback);
   } // end if no error
   else {
      // store an appropriate error message in the
      // interpreter
      Tcl_SetObjResult(activeResolve->interp, 
         create_dnsservice_error(activeResolve->interp, "DNSServiceResolveReply", errorCode));
      result = TCL_ERROR;
   }

   // remove the file handler
   Tcl_DeleteFileHandler(DNSServiceRefSockFD(activeResolve->sdRef));

   // the callback is no longer being used, so decrement the
   // reference count
   Tcl_DecrRefCount(activeResolve->callback);

   // deallocate the resolve service reference
   DNSServiceRefDeallocate(activeResolve->sdRef);

   // deallocate the active_resolve structure
   ckfree((void *)activeResolve);

   if(result == TCL_ERROR) {
      Tcl_BackgroundError(activeResolve->interp);
   }
}

////////////////////////////////////////////////////
// called when a service query record result is received.
// executes the appropriate Tcl callback to let
// the application know what has happened
////////////////////////////////////////////////////
static void bonjour_resolve_address_callback(
   DNSServiceRef sdRef,
   DNSServiceFlags flags,
   uint32_t interfaceIndex,
   DNSServiceErrorType errorCode,
   const char *fullname,
   uint16_t rrtype,
   uint16_t rrclass,
   uint16_t rdlen,
   const void *rdata,
   uint32_t ttl,
   void *context
) {
   active_resolve *activeResolve = (active_resolve *)context;
   int result;

   if(errorCode == kDNSServiceErr_NoError) {
      char ip[16];
      unsigned char *addr = (unsigned char *)rdata;
      sprintf(ip, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);

      // append the service name and domain
      Tcl_ListObjAppendElement(
         activeResolve->interp,
         activeResolve->callback,
         Tcl_NewStringObj(ip, -1));

      // evaluate the callback
      result = Tcl_GlobalEvalObj(activeResolve->interp, 
                                 activeResolve->callback);
   } // end if no error
   else {
      // store an appropriate error message in the
      // interpreter
      Tcl_SetObjResult(activeResolve->interp, 
         create_dnsservice_error(activeResolve->interp, "DNSServiceQueryRecordReply", errorCode));
      result = TCL_ERROR;
   }

   // remove the file handler
   Tcl_DeleteFileHandler(DNSServiceRefSockFD(activeResolve->sdRef));

   // the callback is no longer being used, so decrement the
   // reference count
   Tcl_DecrRefCount(activeResolve->callback);

   // deallocate the resolve service reference
   DNSServiceRefDeallocate(activeResolve->sdRef);

   // deallocate the active_resolve structure
   ckfree((void *)activeResolve);

   if(result == TCL_ERROR) {
      Tcl_BackgroundError(activeResolve->interp);
   }
}
