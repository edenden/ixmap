#!/usr/bin/python

import os
import sys
import json
import radix

class Rib:
	def __init__(self):
		self.rib = {}
		self.rib["unicast-valid"]	= radix.Radix()
		self.rib["unicast-invalid"]	= radix.Radix()
		self.rib["flow-valid"]		= radix.Radix()
		self.rib["flow-invalid"]	= radix.Radix()

	def update(self, prefix, route):
		rib = {}
		node = {}

		if route["protocol"] == "connected":
			rib = self.rib["unicast-valid"]
		else:
			if self.route_validate(route["nexthop"]):
				rib = self.rib["unicast-valid"]
			else:
				rib = self.rib["unicast-invalid"]

		node = rib.search_exact(prefix)
		if not node:
			node = rib.add(prefix)
			node.data["info"] = []

		if self.route_insert(node, route) == 0:		
			if rib == self.rib["unicast-valid"]:
				print "UPDATE: " + prefix + " -> " + \
					self.route_nexthop(node.data["info"][0])
				self.route_find_updated(self.rib["unicast-invalid"])

	def remove(self, prefix, route):
		rib = {}
		node = {}

		if route["protocol"] == "connected":
			rib = self.rib["unicast-valid"]
		else:
			if self.route_validate(route["nexthop"]):
				rib = self.rib["unicast-valid"]
			else:
				rib = self.rib["unicast-invalid"]

		node = rib.search_exact(prefix)
		if not node:
			return

		if self.route_remove(node, route) == 0:
			if rib == self.rib["unicast-valid"]:
				if len(node.data["info"]) == 0:
					rib.delete(prefix)
					print "DELETE: " + prefix
					self.route_find_updated(self.rib["unicast-valid"])
				else:
					print "UPDATE: " + prefix + " -> " + \
						self.route_nexthop(node.data["info"][0])
			else:
				if len(node.data["info"]) == 0:
					rib.delete(prefix)

	def migrate(self, prefix, route, rib_src, rib_dest):
		node = rib_src.search_exact(prefix)
		if not node:
			return

		if self.route_remove(node, route) == 0:
			if rib_src == self.rib["unicast-valid"]:
				if len(node.data["info"]) == 0:
					rib_src.delete(prefix)
					print "DELETE: " + prefix
					self.route_find_updated(self.rib["unicast-valid"])
				else:
					print "UPDATE: " + prefix + " -> " + \
						self.route_nexthop(node.data["info"][0])
			else:
				if len(node.data["info"]) == 0:
					rib_src.delete(prefix)

		node = rib_dest.search_exact(prefix)
		if not node:
			node = rib_dest.add(prefix)
			node.data["info"] = []

		if self.route_insert(node, route) == 0:
			if rib_dest == self.rib["unicast-valid"]:
				print "UPDATE: " + prefix + " -> " + \
					self.route_nexthop(node.data["info"][0])
				self.route_find_updated(self.rib["unicast-invalid"])

	def route_find_updated(self, rib):
		nodes = rib.nodes()
		for node in nodes:
			for route in node.data["info"]:
				if rib == self.rib["unicast-valid"]:
					if route["protocol-id"] != 0 \
					and not self.route_validate(route["nexthop"]):
						self.migrate(node.prefix, route, \
							self.rib["unicast-valid"], \
							self.rib["unicast-invalid"])
						return
				elif rib == self.rib["unicast-invalid"]:
					if self.route_validate(route["nexthop"]):
						self.migrate(node.prefix, route, \
							self.rib["unicast-invalid"], \
							self.rib["unicast-valid"])
						return

	def route_remove(self, node, route):
		i = 0
		for (i, route_current) in enumerate(node.data["info"]):
			if route["id"] == route_current["id"]:
				node.data["info"].pop(i)
				break
		return i

	def route_insert(self, node, route):
		i = 0
		for (i, route_current) in enumerate(node.data["info"]):
			if route["protocol-id"] < route_current["protocol-id"]:
				node.data["info"].insert(i, route)
				return i
			elif route["protocol-id"] == route_current["protocol-id"]:
				if route["protocol-id"] == 0:
					if route_compare_connected(route, route_current):
						node.data["info"].insert(i, route)
						return i
				elif route["protocol-id"] == 1:
					if route_compare_static(route, route_current):
						node.data["info"].insert(i, route)
						return i
				elif route["protocol-id"] == 2:
					if route_compare_bgp(route, route_current):
						node.data["info"].insert(i, route)
						return i
		node.data["info"].insert(i, route)
		return i

	def route_compare_connected(self, route, route_current):
		# TBD
		return 1

	def route_compare_static(self, route, route_current):
		# TBD
		return 1

	def route_compare_bgp(self, route_new, route_cur):
		lp_new = route_new["local-preference"] \
			if "local-preference" in route_new["attribute"] else 100
		lp_cur = route_cur["local-preference"] \
			if "local-preference" in route_cur["attribute"] else 100

		if lp_new > lp_cur:
			return 1
		elif lp_new == lp_cur:
			if route["origin-id"] < route_current["origin-id"]:
				return 1
#			elif route["origin-id"] == route_current["origin-id"]:
				# TBD
		return 0

	def route_validate(self, nexthop):
		node = self.rib["unicast-valid"].search_best(nexthop)

		if not node:
			return 0

		if node.data["info"][0]["protocol-id"] == 0:
			return 1
		else:
			return self.route_validate(node.data["info"][0]["nexthop"])

		return 0

	def route_nexthop(self, route):
		if route["protocol-id"] == 0:
			return "connected"
		else:
			return route["nexthop"]
