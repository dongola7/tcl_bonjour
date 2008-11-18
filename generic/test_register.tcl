lappend auto_path ..

package require bonjour 0.1

set registerList {
   _http._tcp 80 {first blair last kitchen}
   _ssh._tcp 22 {first lucia last laguzzi}
   
}

foreach {regtype port txt} $registerList {
   puts "Registering $regtype on port $port"
   bonjour::register $regtype $port $txt
}

after 10000 [list set runFlag false]

vwait runFlag
