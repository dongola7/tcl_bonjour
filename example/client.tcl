# Example client using the bonjour package.
package require Tcl 8.5
package require bonjour

# Called when a service is resolved (details are retrieved).
proc serviceResolved {serviceName hostname port txtRecords} {
    puts "Resolved: $serviceName on $hostname:$port"
    puts "   txtRecords: $txtRecords"
}

# Called when a service address is resolved.
proc addressResolved {address} {
    puts "Address Resolved: $address"
}

# Called when a service is found.
proc serviceFound {regType action name domain} {
    puts "$regType on $name.$domain: $action"
    if {$action eq "add"} {
        ::bonjour::resolve $name $regType $domain serviceResolved
        ::bonjour::resolve_address $name addressResolved
    }
}

# Start looking for ssh services.
::bonjour::browse start _ssh._tcp [list serviceFound _ssh._tcp]

# Start looking for myservice services.
::bonjour::browse start _myservice._tcp [list serviceFound _myservice._tcp]
::bonjour::browse start _myotherservice._tcp [list serviceFound _myotherservice._tcp]

# Enter the event loop.
vwait forever
