*ip;test-ip4
-*inet;test-inet

# filter chains available are: input, output, forward, prerouting, postrouting
:filter-input;type filter hook input priority 0
:filter-pre;type filter hook prerouting priority 0
:filter-forw;type filter hook forward priority 0
:filter-out;type filter hook output priority 0
:filter-post;type filter hook postrouting priority 0
# nat chains available are: input, output, prerouting, postrouting
:nat-input-t;type nat hook input priority 0
:nat-pre-t;type nat hook prerouting priority 0
:nat-out-t;type nat hook output priority 0
:nat-post-t;type nat hook postrouting priority 0
# route chain available are: output
:route-out-t;type route hook output priority 0

#ip daddr 192.168.0.1-192.168.0.250;ok
#ip daddr 192.168.0.1;ok
#ip daddr 192.168.0.1 drop;ok
#ip daddr 192.168.0.2 log;ok
#ip daddr 192.168.0.2 log;ok
