#!/bin/csh

set i = 1
while ( $i <= 100 )
    echo "Hello World!" > "$i.txt"
    @ i++
end


set j = 1
while ( $j <= 5 )
    cat "$j.txt"
    @ j++
end