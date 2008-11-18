lappend auto_path ..

package require bonjour 0.1

proc browse_callback {regtype action service domain} {
   puts "browse $action $regtype $service $domain"
   if {$action eq "add"} {
      ::bonjour::resolve $service $regtype $domain resolve_callback
   }
}

proc resolve_callback {fullname hosttarget port txtRecords} {
   puts "resolve $fullname $hosttarget $port"
   foreach {key value} $txtRecords {
      puts "\t$key = $value"
   }
}

after 10000 [list set runFlag false]

set serviceTypes {
   _http._tcp
   _ssh._tcp
   _mysql._tcp
   _daap._tcp
   _ipp._tcp
   _presence._tcp
}

foreach serviceType $serviceTypes {
   ::bonjour::browse start $serviceType \
      [list browse_callback $serviceType]
}

vwait runFlag

foreach serviceType $serviceTypes {
   ::bonjour::browse stop $serviceType
}
