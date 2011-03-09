# Example bonjour server
package require Tcl 8.5
package require bonjour

# Turn off any warnings from avahi
global env
set env(AVAHI_COMPAT_NOWARN) 1

# Register myservice with bonjour.
::bonjour::register _myservice._tcp 30000 {key1 value1 key2 value2}

# Register another myservice, but with a specific service name
::bonjour::register_with_name "MyService2 on [info hostname]" _myservice2._tcp 30001 {key1 value1}

# Shutdown after 60 seconds
after 6000 exit

# Enter the event loop.
vwait forever
