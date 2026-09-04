scriptTitle = "Install Aurora A-Z"
scriptAuthor = "haimbilia"
scriptVersion = 1
scriptDescription = "Installs or updates the Aurora A-Z coverflow selector"
scriptIcon = "icon.png"
scriptPermissions = { "filesystem" }

local live = "Hdd1:\\Aurora\\Plugins\\NetDbgDll.xex"
local backup = "Hdd1:\\Aurora\\Plugins\\NetDbgDll.xex.before-aurora-az"
local staged = "Hdd1:\\Aurora\\Plugins\\NetDbgDll.xex.auroraaz-staged"

local function fail(message)
    Script.ShowMessageBox("Aurora A-Z", message, "OK")
end

local function install()
    -- Aurora evaluates top-level metadata before it exposes Script.  Resolve
    -- the package path only after the user launches this script.
    local source = Script.GetBasePath() .. "AuroraAZ.xex"
    if not FileSystem.FileExists(source) then
        fail("AuroraAZ.xex is missing from this script's folder.")
        return
    end

    local choice = Script.ShowMessageBox(
        "Aurora A-Z",
        "Install Aurora A-Z into Aurora's NetDbgDll slot?\n\n" ..
        "An existing NetDbgDll.xex will be renamed to " ..
        "NetDbgDll.xex.before-aurora-az.",
        "Install", "Cancel"
    )
    if choice.Button ~= 1 then return end

    -- Do not disturb the live slot until a complete staged copy exists.
    FileSystem.DeleteFile(staged)
    Script.SetStatus("Staging Aurora A-Z...")
    if FileSystem.CopyFile(source, staged, true) ~= true then
        fail("Could not stage AuroraAZ.xex. The active plugin was not changed.")
        return
    end

    if FileSystem.FileExists(live) then
        FileSystem.DeleteFile(backup)
        Script.SetStatus("Preserving existing NetDbgDll...")
        if FileSystem.MoveFile(live, backup, true) ~= true then
            FileSystem.DeleteFile(staged)
            fail("Could not preserve the existing NetDbgDll.xex. Nothing was installed.")
            return
        end
    end

    Script.SetStatus("Installing Aurora A-Z...")
    if FileSystem.MoveFile(staged, live, true) ~= true then
        if FileSystem.FileExists(backup) then
            FileSystem.MoveFile(backup, live, true)
        end
        FileSystem.DeleteFile(staged)
        fail("Installation failed. The previous plugin was restored when possible.")
        return
    end

    local restart = Script.ShowMessageBox(
        "Aurora A-Z installed",
        "Installation complete. Reboot Aurora to activate the selector.",
        "Later", "Restart Aurora"
    )
    if restart.Button == 2 and type(Aurora.Restart) == "function" then
        Aurora.Restart()
    end
end

function main()
    install()
end
