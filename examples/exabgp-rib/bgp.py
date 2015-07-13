#!/usr/bin/python

import os
import sys
import json
import radix

class Neighbor:
	def __init__(self):
		self.database = {}
		self.database["unicast"]	= radix.Radix()
		self.database["flow"]		= radix.Radix()

        def update(self, prefix, route):
		node = self.database["unicast"].search_exact(prefix)
		if not node:
			node = self.database["unicast"].add(prefix)
			node.data["info"] = []
		node.data["info"].append(route)

	def withdraw(self, prefix, route):
		node = self.database["unicast"].search_exact(prefix)
		if node:
			for (i, route_current) in enumerate(node.data["info"]):
				if route["id"] == route_current["id"]:
					node.data["info"].pop(i)
					break
			if len(node.data["info"]) == 0:
				self.database["unicast"].delete(prefix)

	def dump(self):
		return self.database["unicast"].nodes()

class Bgp:
	def __init__(self):
		self.neighbors = {}

	def route_change(self, rib, data):
		neighbor	= data["neighbor"]
		nbraddr		= neighbor["ip"]
		asn		= neighbor["asn"]
		message		= neighbor["message"]
		type		= message.keys()[0]
		announce	= message[type]["announce"]
		attribute	= message[type]["attribute"]
		family		= announce.keys()[0]

		if family == "ipv4 unicast":
			nexthop = announce[family].keys()[0]
			for prefix in announce[family][nexthop].keys():
				route = {}
				route["protocol"]	= "bgp"
				route["protocol-id"]	= 2
				route["nexthop"]        = nexthop
				route["attribute"]      = attribute
				route["asn"]            = asn
				route["nbraddr"]        = nbraddr
				route["id"]		= "bgp-" + prefix + "-" + nbraddr

				if route["attribute"]["origin"] == "igp":
					route["attribute"]["origin-id"] = 0
				elif route["attribute"]["origin"] == "egp":
					route["attribute"]["origin-id"] = 1
				elif route["attribute"]["origin"] == "incomplete":
					route["attribute"]["origin-id"] = 2
				else:
					route["attribute"]["origin-id"] = 255

				if type == "update":
					self.neighbors[nbraddr].update(prefix, route)
					rib.update(prefix, route)
				elif type == "withdraw":
					self.neighbors[nbraddr].withdraw(prefix, route)
					rib.remove(prefix, route)
#		elif family == "ipv4 flow":

		return

	def nbr_change(self, rib, data):
		neighbor	= data["neighbor"]
		nbraddr		= neighbor["ip"]
		state		= neighbor["state"]

		if state == "up":
			self.neighbors[nbraddr] = Neighbor()

		elif state == "down":
			nodes = self.neighbors[nbraddr].dump()
			for node in nodes:
				for route in node.data["info"]:
					rib.remove(node.prefix, route)
			del self.neighbors[nbraddr]
