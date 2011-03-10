# Example bonjour server
package require Tcl 8.5
package require bonjour

# Register myservice with bonjour.
::bonjour::register _myservice._tcp 30000 {key1 value1 key2 value2}

# Register named service with bonjour
::bonjour::register -name "My Other Service" _myotherservice._tcp 30001 {key1 value1 key2 value2}

# Shutdown after 60 seconds
after 6000 exit

# Enter the event loop.
vwait forever
