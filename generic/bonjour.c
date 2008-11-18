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

/*
*  A Tcl package to allow access to bonjour functionality
*
*  Author: Blair Kitchen <blair@the-blair.com>
*/

/*
* TODO:
*  - Keep track of the active_resolve structures so that
*    they can be deallocated in the event of a call to
*    bonjour_cleanup.  (If the program exits in the
*    middle of a resolution, for example.)
*  - Fix handling of protocol errors returned by bonjour
*    in the Tcl_BackgroundError function calls.  More
*    descriptive error messages are necessary.
*  - Add more error handling around DNS calls.
*/

#include <string.h>

#include <tcl.h>
#include <dns_sd.h>

#include "bonjour.h"

////////////////////////////////////////////////////
// initialize the package
////////////////////////////////////////////////////
int Bonjour_Init(
   Tcl_Interp *interp
) {
   // Initialize the stubs library
   if(Tcl_InitStubs(interp, "8.4", 0) == NULL) {
      return(TCL_ERROR);
   }

   // Tell Tcl what package we're providing
   Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);

   // initialize components
   Browse_Init(interp);
   Register_Init(interp);
   Resolve_Init(interp);

   return(TCL_OK);
}

////////////////////////////////////////////////////
// called by the Tcl event loop when there is data
// on the socket used by the DNS service reference
////////////////////////////////////////////////////
void bonjour_tcl_callback(
   ClientData clientData,
   int mask
) {
   DNSServiceRef sdRef = (DNSServiceRef)clientData;

   // process the incoming data
   DNSServiceProcessResult(sdRef);
}

