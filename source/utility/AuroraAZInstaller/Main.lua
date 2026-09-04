scriptTitle = "Install Aurora A-Z"
scriptAuthor = "haimbilia"
scriptVersion = 2
scriptDescription = "Installs the Aurora A-Z DashLaunch plugin"
scriptIcon = "icon.png"
scriptPermissions = { "filesystem" }

local pluginPath = "Hdd1:\\Aurora\\Plugins\\AuroraAZ.xex"
local oldNetDbgPath = "Hdd1:\\Aurora\\Plugins\\NetDbgDll.xex"
local launchIniPath = "Hdd1:\\launch.ini"
local backupPath = "Hdd1:\\launch.ini.AuroraAZ.backup"
local stagePath = "Hdd1:\\launch.ini.AuroraAZ.stage"
local pluginValue = "Hdd:\\Aurora\\Plugins\\AuroraAZ.xex"

local function fail(message)
    Script.ShowMessageBox("Aurora A-Z", message, "OK")
end

local function readFile(path)
    local ok, file = pcall(io.open, path, "rb")
    if not ok or file == nil then return nil end
    local data = file:read("*a")
    file:close()
    return data
end

local function writeFile(path, data)
    local ok, file = pcall(io.open, path, "wb")
    if not ok or file == nil then return false end
    local written = file:write(data)
    file:close()
    return written ~= nil
end

local function addPluginSlot(contents)
    local lines = {}
    local inPlugins = false
    local installed = false

    -- DashLaunch's INI parser accepts CRLF or LF. Keep all settings and only
    -- replace the first blank plugin1..plugin5 assignment in [Plugins].
    for line in (contents .. "\n"):gmatch("([^\r\n]*)\r?\n") do
        if line:match("^%s*%[.-%]%s*$") then
            inPlugins = line:lower():match("^%s*%[plugins%]%s*$") ~= nil
        end
        if inPlugins and not installed then
            local key = line:match("^(%s*plugin[1-5]%s*)=%s*$")
            if key ~= nil then
                line = key .. "= " .. pluginValue
                installed = true
            end
        end
        table.insert(lines, line)
    end

    if not installed then return nil end
    return table.concat(lines, "\r\n")
end

local function install()
    local source = Script.GetBasePath() .. "AuroraAZ.xex"
    local original
    local updated
    local choice

    if not FileSystem.FileExists(source) then
        fail("AuroraAZ.xex is missing from this script folder.")
        return
    end
    if FileSystem.FileExists(oldNetDbgPath) then
        fail("An old NetDbg-slot Aurora A-Z plugin is active. Disable it before installing the DashLaunch version.")
        return
    end

    original = readFile(launchIniPath)
    if original == nil then
        fail("Could not read Hdd1:\\launch.ini. Nothing was changed.")
        return
    end
    updated = addPluginSlot(original)
    if updated == nil then
        fail("No empty DashLaunch plugin1..plugin5 slot was found. Nothing was changed.")
        return
    end

    choice = Script.ShowMessageBox(
        "Install Aurora A-Z",
        "Install AuroraAZ.xex and add it to the first empty DashLaunch plugin slot?\n\nA backup of launch.ini will be saved first.",
        "Install", "Cancel"
    )
    if choice.Button ~= 1 then return end

    Script.SetStatus("Installing Aurora A-Z plugin...")
    if FileSystem.CopyFile(source, pluginPath, true) ~= true then
        fail("Could not copy AuroraAZ.xex. launch.ini was not changed.")
        return
    end
    FileSystem.DeleteFile(backupPath)
    if FileSystem.CopyFile(launchIniPath, backupPath, true) ~= true then
        fail("Could not back up launch.ini. The plugin was copied but will not load until launch.ini is configured.")
        return
    end
    FileSystem.DeleteFile(stagePath)
    if not writeFile(stagePath, updated) or
        FileSystem.MoveFile(stagePath, launchIniPath, true) ~= true then
        FileSystem.DeleteFile(stagePath)
        fail("Could not update launch.ini. The backup is available at launch.ini.AuroraAZ.backup.")
        return
    end

    Script.ShowMessageBox(
        "Aurora A-Z installed",
        "Installation complete. Restart Aurora manually to activate Aurora A-Z.",
        "OK"
    )
end

function main()
    install()
end
