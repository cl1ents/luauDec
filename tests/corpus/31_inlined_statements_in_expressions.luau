-- O2 inlines `getPlatform` into `getIcon`'s index expression, together with its statements and early returns
local calls = {}
local Service = {}

function Service:GetPlatform()
	table.insert(calls, "GetPlatform")
	if self.fail then
		error("unavailable")
	end
	return "pc"
end

function Service:IsTenFoot()
	table.insert(calls, "IsTenFoot")
	return self.tenFoot
end

function Service.getInputPlatform()
	table.insert(calls, "input")
	return "gamepad"
end

local platforms = { gamepad = "console" }
local icons = { pc = "mouse.png", console = "pad.png" }

local function getPlatform()
	local ok, result = pcall(Service.GetPlatform, Service)
	if ok then
		return result
	end
	local tenFoot = Service:IsTenFoot()
	local input = Service.getInputPlatform()
	if tenFoot then
		return platforms[input]
	end
	return nil
end

local function getIcon()
	return icons[getPlatform()]
end

print(getIcon())
Service.fail = true
print(getIcon())
Service.tenFoot = true
print(getIcon())
print(table.concat(calls, ","))
