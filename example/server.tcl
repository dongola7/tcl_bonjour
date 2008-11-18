# Example bonjour server
lappend auto_path .

package require Tcl 8.4
package require bonjour

# Register myservice with bonjour.
::bonjour::register _myservice._tcp 30000 {key1 value1 key2 value2}

# Enter the event loop.
vwait forever
