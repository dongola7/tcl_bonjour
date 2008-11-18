# Example client using the bonjour package.

lappend auto_path .

package require Tcl 8.4
package require bonjour

# Called when a service is resolved (details are retrieved).
proc serviceResolved {serviceName hostname port txtRecords} {
    puts "Resolved: $serviceName on $hostname:$port"
    puts "   txtRecords: $txtRecords"
}

# Called when a service is found.
proc serviceFound {regType action name domain} {
    puts "$regType on $name.$domain: $action"
    if {$action eq "add"} {
        ::bonjour::resolve $name $regType $domain serviceResolved
    }
}

# Start looking for ssh services.
::bonjour::browse start _ssh._tcp [list serviceFound _ssh._tcp]

# Start looking for myservice services.
::bonjour::browse start _myservice._tcp [list serviceFound _myservice._tcp]

# Enter the event loop.
vwait forever
