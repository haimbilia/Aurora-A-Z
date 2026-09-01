scriptTitle = "Aurora A-Z Installer"
scriptAuthor = "haimbilia"
scriptVersion = 0.3
scriptDescription = "Install, update, or remove the A-Z QuickView backend"
scriptPermissions = { "sql" }

local ownerMarker = "AURORA_AZ"
local versionSetting = "AuroraAZInstalledVersion"
local previousDefaultSetting = "AuroraAZPreviousDefaultQuickView"
local selectorCount = 27

local function execute(query)
    if Sql.Execute(query) ~= true then
        print("Aurora A-Z SQL failed: " .. query)
        return false
    end
    return true
end

local function isInstalled()
    local rows = Sql.ExecuteFetchRows(
        "SELECT Value FROM SystemSettings WHERE Name = '" .. versionSetting .. "' LIMIT 1"
    )
    return rows ~= nil and #rows > 0
end

local function insertQuickView(displayName, filterName, orderIndex)
    local query = string.format(
        "INSERT INTO QuickViews " ..
        "(DisplayName, SortMethod, FilterMethod, Flags, CreatorXUID, OrderIndex, IconHash) " ..
        "VALUES ('%s', 'Title Name', '%s', 2, NULL, %d, '%s')",
        displayName,
        filterName,
        orderIndex,
        ownerMarker
    )
    return execute(query)
end

local function install()
    local updating = isInstalled()

    if execute("BEGIN TRANSACTION") ~= true then
        return false
    end

    local ok = true

    if not updating then
        ok = ok and execute(
            "INSERT OR REPLACE INTO SystemSettings (Name, Value) " ..
            "SELECT '" .. previousDefaultSetting .. "', Value FROM SystemSettings " ..
            "WHERE Name = 'DefaultQuickView'"
        )
        ok = ok and execute(
            "UPDATE QuickViews SET OrderIndex = OrderIndex + " .. selectorCount ..
            " WHERE OrderIndex >= 1"
        )
    end

    ok = ok and execute("DELETE FROM QuickViews WHERE IconHash = '" .. ownerMarker .. "'")
    ok = ok and insertQuickView("#", "A-Z #", 1)

    for code = 65, 90 do
        local letter = string.char(code)
        ok = ok and insertQuickView(letter, "A-Z " .. letter, code - 63)
    end

    ok = ok and execute(
        "UPDATE SystemSettings SET Value = " ..
        "(SELECT CAST(Id AS TEXT) FROM QuickViews WHERE IconHash = 'SHOWALL' ORDER BY Id LIMIT 1) " ..
        "WHERE Name = 'DefaultQuickView'"
    )
    ok = ok and execute(
        "INSERT OR REPLACE INTO SystemSettings (Name, Value) VALUES ('" ..
        versionSetting .. "', '0.3')"
    )

    if ok then
        execute("COMMIT")
        Script.SetRefreshListOnExit(true)
        return true
    end

    execute("ROLLBACK")
    return false
end

local function uninstall()
    if not isInstalled() then
        Script.ShowMessageBox("Aurora A-Z", "The A-Z backend is not installed.", "OK")
        return true
    end

    if execute("BEGIN TRANSACTION") ~= true then
        return false
    end

    local ok = true
    ok = ok and execute("DELETE FROM QuickViews WHERE IconHash = '" .. ownerMarker .. "'")
    ok = ok and execute(
        "UPDATE QuickViews SET OrderIndex = OrderIndex - " .. selectorCount ..
        " WHERE OrderIndex > " .. selectorCount
    )
    ok = ok and execute(
        "UPDATE SystemSettings SET Value = " ..
        "(SELECT Value FROM SystemSettings WHERE Name = '" .. previousDefaultSetting .. "') " ..
        "WHERE Name = 'DefaultQuickView' AND EXISTS " ..
        "(SELECT 1 FROM SystemSettings WHERE Name = '" .. previousDefaultSetting .. "')"
    )
    ok = ok and execute(
        "DELETE FROM SystemSettings WHERE Name IN ('" .. versionSetting ..
        "', '" .. previousDefaultSetting .. "')"
    )

    if ok then
        execute("COMMIT")
        Script.SetRefreshListOnExit(true)
        return true
    end

    execute("ROLLBACK")
    return false
end

local function offerRestart(message)
    local result = Script.ShowMessageBox(
        "Aurora A-Z",
        message .. "\n\nAurora must restart before the change is active.",
        "Later",
        "Restart"
    )
    if result.Button == 2 then
        Aurora.Restart()
    end
end

function main()
    local action = Script.ShowMessageBox(
        "Aurora A-Z",
        "Install/update the # through Z selector backend, or restore the previous QuickView configuration?",
        "Install / Update",
        "Uninstall",
        "Cancel"
    )

    if action.Button == 1 then
        if install() then
            offerRestart("The A-Z QuickViews were installed successfully.")
        else
            Script.ShowMessageBox("Aurora A-Z", "Installation failed; database changes were rolled back.", "OK")
        end
    elseif action.Button == 2 then
        if uninstall() then
            offerRestart("The A-Z QuickViews were removed and the previous default was restored.")
        else
            Script.ShowMessageBox("Aurora A-Z", "Uninstall failed; database changes were rolled back.", "OK")
        end
    end
end
