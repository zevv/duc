#!/usr/bin/lua

Duc = require "lduc"

-- Create duc context

duc = Duc:new()

-- Open database. The default database path is used if no file name is given

duc:open()

-- Open /tmp directory and list all files

dir = duc:dir_open("/tmp")

while true do
	ent = dir:read()
	if not ent then
		break
	end
	print(string.format("%-5.5s %7d %-30.30s %12d", 
		ent.type, ent.size.count, ent.name, ent.size.actual))
end

-- No need to close duc state, garbage collector does that for us

-- vi: ft=lua ts=3 sw=3
