scriptTitle = "Aurora A-Z Installer"
scriptAuthor = "haimbilia"
scriptVersion = 0.4
scriptDescription = "Install, update, or remove the A-Z QuickView selector"
scriptPermissions = { "sql" }

local ownerMarker = "AURORA_AZ"
local versionSetting = "AuroraAZInstalledVersion"
local previousDefaultSetting = "AuroraAZPreviousDefaultQuickView"
local installedVersion = "0.4"
local selectorCount = 27

-- Aurora registers filters under fully qualified names. The boot log prints the
-- exact identifiers, e.g. "NameFilter.G - L.G" and "User.A-Z G". A QuickView's
-- FilterMethod must use that full name; a bare "A-Z G" is rejected with
-- "invalid filter method (syntax error)".
--
-- Aurora already ships a complete built-in initial-character filter set under
-- NameFilter, so the letters point at native filters rather than our Lua ones.
local function nameFilterFor(letter)
    if letter <= "F" then return "NameFilter.A - F." .. letter end
    if letter <= "L" then return "NameFilter.G - L." .. letter end
    if letter <= "R" then return "NameFilter.M - R." .. letter end
    if letter <= "X" then return "NameFilter.S - X." .. letter end
    return "NameFilter.Y - Z." .. letter
end

local function requestRefreshOnExit()
    -- The API dump confirmed the name is RefreshListOnExit. The earlier
    -- SetRefreshListOnExit call was the source of the LUAERROR on first install.
    if type(Script.RefreshListOnExit) == "function" then
        Script.RefreshListOnExit(true)
    end
end

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

-- IconHash doubles as the ownership marker. It may cost a placeholder icon in
-- the picker, but it is the only field that reliably identifies our rows, and
-- deleting by it can never touch a QuickView the user created.
local function insertQuickView(displayName, filterName, orderIndex)
    return execute(string.format(
        "INSERT INTO QuickViews " ..
        "(DisplayName, SortMethod, FilterMethod, Flags, CreatorXUID, OrderIndex, IconHash) " ..
        "VALUES ('%s', 'Title Name', '%s', 2, NULL, %d, '%s')",
        displayName,
        filterName,
        orderIndex,
        ownerMarker
    ))
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

    -- Must match uninstall exactly. Matching on the marker alone let an
    -- earlier marker-less build survive a reinstall and produce a duplicate
    -- set of every letter.
    ok = ok and execute(
        "DELETE FROM QuickViews WHERE IconHash = '" .. ownerMarker .. "' " ..
        "OR (FilterMethod LIKE 'NameFilter.%' AND length(DisplayName) = 1)"
    )

    ok = ok and insertQuickView("#", "NameFilter.Other", 1)

    for code = 65, 90 do
        local letter = string.char(code)
        ok = ok and insertQuickView(letter, nameFilterFor(letter), code - 63)
    end

    -- Only reset the default view if the Show All entry actually exists.
    ok = ok and execute(
        "UPDATE SystemSettings SET Value = " ..
        "(SELECT CAST(Id AS TEXT) FROM QuickViews WHERE IconHash = 'SHOWALL' ORDER BY Id LIMIT 1) " ..
        "WHERE Name = 'DefaultQuickView' " ..
        "AND EXISTS (SELECT 1 FROM QuickViews WHERE IconHash = 'SHOWALL')"
    )
    ok = ok and execute(
        "INSERT OR REPLACE INTO SystemSettings (Name, Value) VALUES ('" ..
        versionSetting .. "', '" .. installedVersion .. "')"
    )

    if ok then
        execute("COMMIT")
        requestRefreshOnExit()
        return true
    end

    execute("ROLLBACK")
    return false
end

local function uninstall()
    if not isInstalled() then
        Script.ShowMessageBox("Aurora A-Z", "The A-Z selector is not installed.", "OK")
        return true
    end

    if execute("BEGIN TRANSACTION") ~= true then
        return false
    end

    local ok = true

    -- Two generations of rows exist: early ones carry the IconHash marker,
    -- and one build wrote them with an empty IconHash. Catch both. The
    -- single-character DisplayName keeps this off any NameFilter QuickView
    -- the user made themselves, which would have a real name.
    ok = ok and execute(
        "DELETE FROM QuickViews WHERE IconHash = '" .. ownerMarker .. "' " ..
        "OR (FilterMethod LIKE 'NameFilter.%' AND length(DisplayName) = 1)"
    )
    ok = ok and execute(
        "UPDATE QuickViews SET OrderIndex = OrderIndex - " .. selectorCount ..
        " WHERE OrderIndex > " .. selectorCount
    )

    -- Only put the old default back if the current one was one of ours and is
    -- now gone. If the user has since chosen a different view, keep theirs.
    ok = ok and execute(
        "UPDATE SystemSettings SET Value = " ..
        "(SELECT Value FROM SystemSettings WHERE Name = '" .. previousDefaultSetting .. "') " ..
        "WHERE Name = 'DefaultQuickView' " ..
        "AND EXISTS (SELECT 1 FROM SystemSettings WHERE Name = '" .. previousDefaultSetting .. "') " ..
        "AND NOT EXISTS (SELECT 1 FROM QuickViews WHERE CAST(Id AS TEXT) = " ..
        "(SELECT Value FROM SystemSettings WHERE Name = 'DefaultQuickView'))"
    )
    ok = ok and execute(
        "DELETE FROM SystemSettings WHERE Name IN ('" .. versionSetting ..
        "', '" .. previousDefaultSetting .. "')"
    )

    if ok then
        execute("COMMIT")
        requestRefreshOnExit()
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
        if type(Aurora.Restart) == "function" then
            Aurora.Restart()
        else
            Script.ShowMessageBox(
                "Aurora A-Z",
                "Automatic restart is unavailable in this Aurora build. Restart Aurora manually.",
                "OK"
            )
        end
    end
end

function main()
    local action = Script.ShowMessageBox(
        "Aurora A-Z",
        "Install the # through Z selector using Aurora's built-in NameFilter set, " ..
        "or restore the previous QuickView configuration?",
        "Install",
        "Uninstall",
        "Cancel"
    )

    if action.Button == 1 then
        if install() then
            offerRestart("The A-Z QuickViews were installed.")
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
