#!/usr/bin/python

import os
import sys
import json
import bgp
import rib

if __name__ == "__main__":
	rib = rib.Rib()
	bgp = bgp.Bgp()

	route = {}
	route["protocol"]	= "connected"
	route["protocol-id"]	= 0
	route["id"]		= "connected-1"
	rib.update("203.178.138.64/27", route)

	#route = {}
	#route["protocol"]	= "static"
	#route["protocol-id"]	= 1
	#route["id"]		= "static-1"
	#route["nexthop"]	= "203.178.138.65"
	#rib.update("0.0.0.0/0", route)

	for line in sys.stdin:
		data = json.loads(line)
		if data["type"] == "state":
			bgp.nbr_change(rib, data)
		elif data["type"] == "update":
			bgp.route_change(rib, data)

