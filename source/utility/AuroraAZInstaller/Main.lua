scriptTitle = "Install Aurora A-Z"
scriptAuthor = "haimbilia"
scriptVersion = 3
scriptDescription = "Installs or updates Aurora A-Z v1.0 (NetDbg loader)"
scriptIcon = "icon.png"
scriptPermissions = { "filesystem" }

local live = "Hdd1:\\Aurora\\Plugins\\NetDbgDll.xex"
local backupBase = live .. ".before-aurora-az"
local staged = live .. ".auroraaz-staged"

local function message(text)
    Script.ShowMessageBox("Aurora A-Z", text, "OK")
end

local function unusedBackup()
    if not FileSystem.FileExists(backupBase) then return backupBase end
    for index = 1, 100 do
        local path = backupBase .. "." .. index
        if not FileSystem.FileExists(path) then return path end
    end
    return nil
end

function main()
    -- Script is unavailable while Aurora scans top-level menu metadata.
    local source = Script.GetBasePath() .. "AuroraAZ.xex"
    if not FileSystem.FileExists(source) then
        message("AuroraAZ.xex is missing from this script's folder.")
        return
    end
    local choice = Script.ShowMessageBox("Install Aurora A-Z",
        "Install Aurora A-Z into Aurora's NetDbgDll slot?\n\n" ..
        "An existing file will be renamed to a backup. launch.ini and skins are not changed.",
        "Install", "Cancel")
    if choice == nil or choice.Button ~= 1 then return end

    local backup = nil
    if FileSystem.FileExists(live) then
        backup = unusedBackup()
        if backup == nil then
            message("No free backup filename. Nothing was changed.")
            return
        end
    end

    FileSystem.DeleteFile(staged)
    Script.SetStatus("Staging Aurora A-Z...")
    if FileSystem.CopyFile(source, staged, true) ~= true then
        FileSystem.DeleteFile(staged)
        message("Could not stage AuroraAZ.xex. The active plugin was not changed.")
        return
    end
    if backup ~= nil then
        Script.SetStatus("Preserving existing NetDbgDll...")
        if FileSystem.MoveFile(live, backup, true) ~= true then
            FileSystem.DeleteFile(staged)
            message("Could not preserve the existing NetDbgDll.xex. Nothing was installed.")
            return
        end
    end
    Script.SetStatus("Installing Aurora A-Z...")
    if FileSystem.MoveFile(staged, live, true) ~= true then
        local restored = backup == nil or FileSystem.MoveFile(backup, live, true) == true
        FileSystem.DeleteFile(staged)
        if restored then
            message("Installation failed. The previous plugin was restored, if present.")
        else
            message("Installation failed. Restore the previous plugin from:\n" .. backup)
        end
        return
    end
    local text = "Installation complete. Reboot the console manually, then hold R3 on the coverflow.\n\nHold R3 and click L3 to switch Browse/Filter mode."
    if backup ~= nil then text = text .. "\n\nBackup: " .. backup end
    message(text)
    -- Do not invoke Aurora's script restart API: it black-screened on hardware.
end
